#!/usr/bin/python3
# -*- coding: utf-8 -*-

from flask import Flask, redirect, render_template, Response, request, session

import argparse
import cv2
import datetime as dt
import flask
import hashlib
import importlib
import io
import json
import logging
import numpy as np
import os
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
    SESSION_COOKIE_SECURE=True,
    SESSION_COOKIE_HTTPONLY=True,
    SESSION_COOKIE_SAMESITE='Lax',
)


INVALID_VALUE = -65536

# app_address: the app's address (including protocol and port) on the Internet
app_address = ''
# app_dir: the app's real address on the filesystem
app_dir = os.path.dirname(os.path.realpath(__file__))
door_open_db = os.path.join(app_dir, 'door.sqlite')
emailer = None
# 28.0301a279faf2 is the shorter one in the rack
# 28-030997792b61 is the shortest one in the rack
# 28-01144ebe52aa and 28-01144ef1faaa are the 2-meter long ones
sensors = ['/sys/bus/w1/devices/28-0301a279faf2/',
           '/sys/bus/w1/devices/28-030997792b61/',
           '/sys/bus/w1/devices/28-01144ebe52aa/',
           '/sys/bus/w1/devices/28-01144ef1faaa/']
stop_signal = False
locations = ['rack-front', 'rack-back', 'rack-ambient1', 'rack-ambient2']
users_path = os.path.join(app_dir, 'users.json')
fans_mode_path = os.path.join(app_dir, 'fans.mode')
fans_mode_changed = True
fans_load_tuned = None
settings = None
settings_path = os.path.join(app_dir, 'settings.json')
th_email = None
temperatures = [0, 0, 0, 0]
# Fill temperatures with 0 can avoid None errors 
# if real readings from sensors are not fetched.


class streamer(object):
    jpeg_bytes = []
    frames = []
    skip = []
    thread_count = -1
    loop_threads = []
    ipcam_urls = []
    ipcam_names = []
    rotate_settings = []
    error_count = []
    height, width = [], []

    @staticmethod
    def start_streaming(ipcam_url: str,
                        ipcam_name: str,
                        rotate: int,
                        video_width: int,
                        video_height: int):

        streamer.thread_count += 1
        streamer.loop_threads.append(None)
        streamer.loop_threads[streamer.thread_count] = threading.Thread(target=streamer.interal_loop)

        streamer.ipcam_urls.append(None)
        streamer.ipcam_urls[streamer.thread_count] = ipcam_url

        streamer.ipcam_names.append(None)
        streamer.ipcam_names[streamer.thread_count] = ipcam_name

        streamer.rotate_settings.append(0)
        streamer.rotate_settings[streamer.thread_count] = rotate

        streamer.height.append(0)
        streamer.height[streamer.thread_count] = video_height
        streamer.width.append(0)
        streamer.width[streamer.thread_count] = video_width

        streamer.jpeg_bytes.append(None)

        streamer.frames.append(None)
        streamer.error_count.append(0)
        streamer.skip.append(20)

        streamer.loop_threads[streamer.thread_count].start()

    @staticmethod
    def get_frame(thread_id: int):
        if (thread_id >= len(streamer.frames)
                or streamer.frames[thread_id] is None):
            return False, None

        return True, streamer.frames[thread_id]

    @staticmethod
    def get_thread_count():
        return streamer.thread_count

    @staticmethod
    def interal_loop():

        global stop_signal

        thread_id = streamer.thread_count
        vcap = None
        vcap_retry = 0
        logging.info('[{}] Interal loop started. ipcam url = {}'
                     .format(thread_id, streamer.ipcam_urls[thread_id]))

        while stop_signal is False:

            if vcap is None or vcap.isOpened() is False:
                vcap_retry += 1
                logging.info(
                        '[{}] vcap not opened, try opening url [{}], retry: {}'
                        .format(thread_id,
                                streamer.ipcam_urls[thread_id],
                                vcap_retry))
                vcap = cv2.VideoCapture(streamer.ipcam_urls[thread_id])
                if vcap.isOpened() is False:
                    streamer.error_count[thread_id] += 1
                    logging.error('[{}] cannot open vcap, waiting {} seconds before retry before continue the loop. error_count: {}'.format(thread_id, streamer.error_count[thread_id], streamer.error_count[thread_id]))
                    time.sleep(streamer.error_count[thread_id])
                if vcap.isOpened():
                    logging.info(f'[{thread_id}] vcap opened.')
                    #  property cv2.CAP_PROP_FRAME_WIDTH is inaccurate
                    vcap_retry = 0

            if vcap is not None and vcap.isOpened:
                i = 0
                while i < streamer.skip[thread_id]:
                    retval, frame = vcap.read()
                    i += 1
            else:
                retval = False
                frame = None
            # error message: [mpjpeg @ ] Expected boundary '--' not found, instead found a line of
            # is generated from the read() function. This message, according to my observation, is closely
            # related to "green screen" error.
            # As I understand, this error is caused by lower-than-necessary network speed (rpi-corridor
            # is sending data at 10MB/S and it seems that this is not fast enough)
            if retval is False:
                streamer.error_count[thread_id] += 1
                logging.error('[{}] Interal loop cannot get new frame from '
                              'vcap.read(), waiting {} seconds before '
                              'continue the loop. error_count: {}'
                              .format(thread_id,
                                      streamer.error_count[thread_id],
                                      streamer.error_count[thread_id]))
                time.sleep(streamer.error_count[thread_id])
                vcap.release()
                # There are occurrences that vcap is opened but vcap.read()
                # returns false. So to make it simple,
                # just release it and retry again.
                frame = np.zeros(shape=[streamer.height[thread_id], streamer.width[thread_id], 3],
                                 dtype=np.uint8)
                # Initialize an empty frame to show error message
                frame[:] = (50, 50, 50)
                # This is to set background color
            else:
                streamer.error_count[thread_id] = 0

            height, width = frame.shape[:2]
            if streamer.width[thread_id] != width or streamer.height[thread_id] != height:
                frame = cv2.resize(frame, (streamer.width[thread_id], streamer.height[thread_id]))

            if streamer.rotate_settings[thread_id] != 0:
                frame = streamer.rotateImage(image=frame, angle=streamer.rotate_settings[thread_id])

            text = 'Time: {}\nLocation: {}'.format(
                    dt.datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                    streamer.ipcam_names[thread_id])
            if retval is False:
                text += ('\nUnable to read image from video device. Retry: {}'
                         .format(streamer.error_count[thread_id]))
            y0, dy = 20, 20
            for i, line in enumerate(text.split('\n')):
                y = y0 + i*dy
                cv2.putText(img=frame, text=line, org=(15, y),
                            fontFace=cv2.FONT_HERSHEY_DUPLEX,
                            fontScale=0.55, color=(1, 1, 1), thickness=2)
                cv2.putText(img=frame, text=line, org=(15, y),
                            fontFace=cv2.FONT_HERSHEY_DUPLEX,
                            fontScale=0.55, color=(255, 255, 255), thickness=1)
            # https://docs.opencv.org/2.4/modules/imgproc/doc/geometric_transformations.html

            streamer.frames[thread_id] = frame

    @staticmethod
    def rotateImage(image, angle):
        image_center = tuple(np.array(image.shape[1::-1]) / 2)
        rot_mat = cv2.getRotationMatrix2D(image_center, angle, 1.0)
        rotated_result = cv2.warpAffine(image, rot_mat, image.shape[1::-1],
                                        flags=cv2.INTER_LINEAR)
        return rotated_result

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
        fans_mode = int(request.form.get("fans_mode", None))
        if fans_mode > 100:
            fans_mode = 100
        if fans_mode < 0:
            fans_mode = -1
    except Exception as e:
        print('???')
        return Response(f'invalid fans_mode [{fans_mode}], {e}', 400)

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

    try:
        json_file = open(fans_mode_path)
        json_str = json_file.read()
        fans_mode = int(json.loads(json_str)['fans_mode'])
    except Exception as e:
        fans_mode = None
        logging.error(e)
        return Response(f'Unable to read from json file', 500)
        
    kwargs = {
        "username": username,
        "app_address": app_address,
        "fans_mode": fans_mode,
        "fans_load_tuned": fans_load_tuned,
        "door_events_table": get_door_events_table(),
        "temperatures": [int(x) for x in temperatures]
    }


    return render_template('index.html', **kwargs)
                           # int() allows temperatures to fit into small screens

def door_sensor_loop():
    
    #GPIO.setmode(GPIO.BCM)
    # Set in fans_controller_loop()

    GPIO_pin_positive = 19
    GPIO.setup(GPIO_pin_positive, GPIO.OUT)
    GPIO.output(GPIO_pin_positive, GPIO.HIGH)

    GPIO_pin_negative = 16
    GPIO.setup(GPIO_pin_negative, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    # When a GPIO pin is in input mode and not connected to 3.3 volts or ground, 
    # the pin is said to be floating, meaning that it has no fixed voltage level.
    # the pin will randomly float between HIGH and LOW.

    conn = sqlite3.connect(door_open_db)
    # No try catch here: if there is an error, the program should not continue
    cur = conn.cursor()

    cur.execute('''
        CREATE TABLE IF NOT EXISTS history
        (
            [event_id] INTEGER PRIMARY KEY AUTOINCREMENT,
            [event_time] TEXT,
            [status] INTEGER
        )
    ''')
    conn.commit()


    last_status = 1    

    global stop_signal
    while stop_signal is False:
        
        current_status = GPIO.input(GPIO_pin_negative)
        if current_status != last_status:
            curr_time = dt.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            if current_status == 1:                
                logging.info('Door is closed')
                cur.execute('''
                    INSERT INTO history (event_time, status)
                    VALUES(?, 0);
                ''',  [curr_time])                
            else:
                logging.info('Door is opened')
                cur.execute('''
                    INSERT INTO history (event_time, status)
                    VALUES(?, 1);
                ''',  [curr_time])
                threading.Thread(
                    target=emailer.send,
                    kwargs={
                        'from_host': settings['email']['from_host'],
                        'from_port': settings['email']['from_port'],
                        'from_name': settings['email']['from_name'],
                        'from_address': settings['email']['from_address'],
                        'from_password': settings['email']['from_password'],
                        'to_name': settings['email']['to_name'],
                        'to_address': settings['email']['to_address'],
                        'subject': 'Server rack door opened',
                        'mainbody': f'Server rack door opened on {curr_time}'
                    }).start()
            conn.commit()

            last_status = current_status              

        time.sleep(1)

    GPIO.cleanup()
    # cleanup() multiple times leads to an warning but is okay
    conn.close()

def fans_controller_loop():

    global fans_mode_changed
    global stop_signal

    sample_interval = 300
    loop_unit_delay = 1
    sample_count = 0
    gpio_pin = 23
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(gpio_pin, GPIO.OUT)

    pwm = GPIO.PWM(gpio_pin, 50)  # Frequency is 50Hz
    pwm.start(0)

    while stop_signal is False:

        time.sleep(loop_unit_delay)
        sample_count += 1
        if (loop_unit_delay * sample_count < sample_interval and
                fans_mode_changed is False):
            continue

        sample_count = 0
        fans_mode_changed = False

        for i in range(0, len(sensors)):
            if stop_signal is True:
                break
            try:
                with open(os.path.join(sensors[i], 'w1_slave'), 'r') as f:
                    data = f.read()

                    if "YES" in data:
                        (discard, sep, reading) = data.partition(' t=')
                        temperatures[i] = round(float(reading) / 1000.0, 1)
                    else:
                        temperatures[i] = INVALID_VALUE
                        logging.error('Failed to read temperature from '
                                      f'sensor_{i}')
                    logging.debug(f'temperature from sensor{i}: '
                                  f'{temperatures[i]}')
            except Exception as e:
                # in case temperature sensors are disconnected
                temperatures[i] = INVALID_VALUE

        high_temperature = abs(temperatures[1] - temperatures[0]) * 0.25 + (temperatures[1] + temperatures[0]) / 2
        # abs(temperatures[1] - temperatures[0]) * 0.25 means
        # fans load should be even higher if the readings from two sensors
        # are more different
        low_temperature = (temperatures[2] + temperatures[3]) / 2
        delta = round(high_temperature - low_temperature, 1)
        # f(delta) = 2 * delta - 10 so that we map [5, 10]
        # to [0, 10]. That is, if delta is less than 5, load_raw will be 0;
        # if delta is 10, load_raw will be 100
        load_raw = int((2 * delta - 10) * 100 / 10)
        # if delta <= 5, fans will be turned off.
        load_tuned = load_raw
        # load_tuned is always calculated, suppose fans_mode is -1 it will
        # be overridden
        if load_tuned > 100:
            load_tuned = 100
        if load_tuned < 25 and load_tuned >= 12.5:
            load_tuned = 25
        if load_tuned < 12.5:
            # fans won't move if voltage is too low anyway
            load_tuned = 0

        try:
            json_file = open(fans_mode_path)
            json_str = json_file.read()
            fans_mode = int(json.loads(json_str)['fans_mode'])
        except Exception as e:
            logging.error(f'Failed to load new fans_mode: {e}')
            fans_mode = -1
        if fans_mode != -1:
            load_tuned = fans_mode
        logging.debug(f'delta: {delta}, load_raw: {load_raw}, '
                      f'fans_mode: {fans_mode}, load_tuned: {load_tuned}')

        for i in range(len(temperatures)):
            try:
                url = ('{}?device_name=rpi-rack&device_token={}&'
                       'data_type=temperature&reading={:1f}&sampling_point={}'
                       .format(
                            settings['app']['telemetry']['url'],
                            settings['app']['telemetry']['device_token'],
                            temperatures[i], locations[i])
                    )
                start = dt.datetime.now()
                r = requests.get(url=url)
                response_timestamp = dt.datetime.now()
                response_time = int((response_timestamp - start).
                                    total_seconds() * 1000)

                logging.info(f'response_text: {r.text}, '
                             f'status_code: {r.status_code}, '
                             f'response_time: {response_time}ms')

            except Exception as e:
                logging.error(f'{e}')
        try:
            url = ('{}?device_name=rpi-rack&device_token={}&'
                   'data_type=fan_load&reading={:1f}&'
                   'sampling_point={}'
                   .format(
                        settings['app']['telemetry']['url'],
                        settings['app']['telemetry']['device_token'],
                        load_tuned, 
                        settings['app']['telemetry']['fans_sampling_point']))
            start = dt.datetime.now()
            r = requests.get(url=url)
            response_timestamp = dt.datetime.now()
            response_time = int((response_timestamp - start).
                                total_seconds() * 1000)
            logging.info(f'response_text: {r.text}, '
                         f'status_code: {r.status_code}, '
                         f'response_time: {response_time}ms')
        except Exception as e:
            logging.error(f'{e}')

        pwm.ChangeDutyCycle(load_tuned)
        global fans_load_tuned
        fans_load_tuned = load_tuned
    pwm.ChangeDutyCycle(0)
    GPIO.cleanup()
    # cleanup() multiple times leads to an warning but is okay


def cleanup(*args):

    global stop_signal
    stop_signal = True
    # GPIO.cleanup()
    # This line is believed to be unnecessary since it has executed
    # in the loop thread
    th_email.stop_thread = True
    logging.info('Stop signal received, exiting')
    sys.exit(0)



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
    emailer = importlib.machinery.SourceFileLoader(
                    'emailer',
                    settings['email']['path']
                ).load_module()
    logging.basicConfig(
        filename=settings['app']['log_path'],
        level=logging.DEBUG if debug_mode else logging.INFO,
        format='%(asctime)s.%(msecs)03d %(levelname)s %(module)s - %(funcName)s: %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S',
    )
    logging.info('Rack Monitor started') 



    
    global th_email
    th_email = threading.Thread(target=emailer.send_service_start_notification,
                                kwargs={'settings_path': os.path.join(app_dir, 'settings.json'),
                                        'service_name': "Rack Monitor",
                                        'log_path': settings['app']['log_path'],
                                        'delay': 0 if debug_mode else 300})
    th_email.start()

    threading.Thread(
        target=streamer.start_streaming(
        ipcam_url=(settings['app']['device_path']),
        ipcam_name='Rack',
        rotate=settings['app']['video_rotate'],
        video_width=settings['app']['video_width'],
        video_height=settings['app']['video_height'])).start()
    # 640 x 480 is the default resolution of the usbcam
    # although according to v4l2-ctl --list-formats-ext
    # it can support a lot others
    threading.Thread(target=fans_controller_loop, args=()).start()
    threading.Thread(target=door_sensor_loop, args=()).start()

    waitress.serve(app, host="127.0.0.1", port=88)
    logging.info('Rack Monitor finished')


if __name__ == '__main__':

    main()
