#!/usr/bin/python3

from emailer import emailer
from flask import Flask, redirect, render_template, Response, request, session

import argparse
import cv2
import datetime as dt
import flask
import hashlib
import io
import json
import logging
import numpy as np
import os
import Pi7SegPy
import requests
import RPi.GPIO as GPIO
import signal
import smtplib
import sqlite3
import sys
import threading
import time
import waitress

app = Flask(__name__)
app.secret_key = b''
app.config.update(
    SESSION_COOKIE_SECURE=False,
    SESSION_COOKIE_HTTPONLY=True,
    SESSION_COOKIE_SAMESITE='Lax',
)


INVALID_VALUE = -65536

# app_address: the app's address (including protocol and port) on the Internet
app_address = ''
# app_dir: the app's real address on the filesystem
app_dir = os.path.dirname(os.path.realpath(__file__))
door_open_db = os.path.join(app_dir, 'door.sqlite')

users_path = os.path.join(app_dir, 'users.json')
settings = None
settings_path = os.path.join(app_dir, 'settings.json')


@app.route('/logout/')
def logout():

    session.pop('username', None)
    return redirect(app_address)


@app.before_request
def make_session_permanent():
    session.permanent = True
    app.permanent_session_lifetime = dt.timedelta(days=90)


@app.route('/login/', methods=['GET', 'POST'])
def login():

    if 'username' in session:
        return redirect(app_address)

    if request.method == 'POST':

        username = request.form['username']
        password = request.form['password'].encode('utf-8')

        try:
            with open(users_path, 'r') as json_file:
                json_str = json_file.read()
                settings = json.loads(json_str)
        except Exception as e:
            return render_template(
                    'login.html',
                    message=f'<span style="color:red">Error: {e}</span>')

        if username not in settings['users']:
            return render_template(
                    'login.html',
                    message=(f'<span style="color:red">'
                             'Error: {username} does not exist</span>'))
        if (hashlib.sha256(password).hexdigest()
                != settings['users'][username]):
            return render_template(
                    'login.html',
                    message=('<span style="color:red">'
                             'Error: Password incorrect</span>'))

        session['username'] = request.form['username']
        return redirect(app_address)

    return render_template('login.html', message='')

@app.route('/live_image/')
def live_image():

    if 'username' in session:
        pass
    else:
        return Response('Not logged in', 400)

    retval, frame = streamer.get_frame(0)
    if retval is True:
        retval, img = cv2.imencode('.jpg',
                                   frame,
                                   [int(cv2.IMWRITE_JPEG_QUALITY), 50])
        img = img.tobytes()
        if retval is False:
            img = b''
    else:
        img = b''
    return flask.send_file(
        io.BytesIO(img),
        mimetype='image/jpeg',
        attachment_filename='live.jpg')


@app.route('/control/', methods=['POST'])
def control():

    if 'username' in session:
        pass
    else:
        return Response('Not logged in', 400)

    try:
        fans_mode = int(request.form.get("fans_mode", 0))
        if fans_mode > 100:
            fans_mode = 100
        if fans_mode < 0:
            fans_mode = -1
    except Exception as e:
        logging.error(e)
        return Response(f'invalid fans_mode', 400)

    fm = {'fans_mode': fans_mode}
    try:
        with open(fans_mode_path, 'w+') as json_file:
            json.dump(fm, json_file)
            global fans_mode_changed
            fans_mode_changed = True
            logging.info(f'fans_mode changed to {fans_mode}')
    except Exception as e:
        logging.error(f'Failed to save new fans_mode: {e}')
        return Response(f'Failed to save new fans_mode.', 500)

    return Response(f'Success: fans_mode changed to {fans_mode}.', 200)


def get_door_events_table():

    table_html = """
    <table class="w3-table w3-striped w3-bordered">
      <tr>
        <th>Event ID</th><th>Event Time</th><th>Door Status</th>
      </tr>
    """
    rows = []
    try:
        conn = sqlite3.connect(door_open_db)
        cur = conn.cursor()

        cur.execute('''SELECT * FROM history ORDER BY event_time DESC LIMIT 5;''')
        rows = cur.fetchall()
    except Exception as e:
        logging.error(e)
        table_html += f'Internal Error'
    else:
        for row in rows:
            table_html += '<tr>'
            table_html += f'<td class="w3-border">{row[0]}</td>'
            table_html += f'<td class="w3-border">{row[1]}</td>'
            if row[2] == 0:
                door_status = '<span style="color:green">Closed</span>'
            else:
                door_status = '<span style="color:red">Opened</span>'
            table_html += f'<td class="w3-border" style="font-weight:bold">{door_status}</td>'
            table_html += '</tr>'

    table_html += '</table>'

    return table_html

@app.route('/', methods=['GET', 'POST'])
def index():

    if 'username' in session:
        username = session['username']
    else:
        return redirect(f'{app_address}/login/')
    temperatures = [1, 2, 3, 4]
    kwargs = {
        "username": username,
        "app_address": app_address,
        "fans_mode": 0,
        "fans_load_tuned": 0,
        "door_events_table": get_door_events_table(),
        "temperatures": [int(x) for x in temperatures]
    }


    return render_template('index.html', **kwargs)
                           # int() allows temperatures to fit into small screens


def main():

    ap = argparse.ArgumentParser()
    ap.add_argument('--debug', dest='debug', action='store_true')
    args = vars(ap.parse_args())
    debug_mode = args['debug']

    signal.signal(signal.SIGINT, cleanup)
    signal.signal(signal.SIGTERM, cleanup)

    start_time = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")    

    global settings
    global app_address, emailer
    with open(settings_path, 'r') as json_file:
        json_str = json_file.read()
        settings = json.loads(json_str)
    app.secret_key = settings['flask']['secret_key']
    app_address = settings['app']['app_address']
    logging.basicConfig(
        filename=settings['app']['log_path'],
        level=logging.DEBUG if debug_mode else logging.INFO,
        format='%(asctime)s.%(msecs)03d %(levelname)s %(module)s - %(funcName)s: %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S',
    )
    logging.info('Rack Monitor started') 
    
    waitress.serve(app, host="0.0.0.0", port=80)
    logging.info('Rack Monitor finished')


if __name__ == '__main__':

    main()
