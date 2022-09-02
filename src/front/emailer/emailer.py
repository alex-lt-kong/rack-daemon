#!/usr/bin/python3
# -*- coding: utf-8 -*-

import datetime as dt
import json
import logging
import os
import smtplib
import threading
import time


def send_service_start_notification(
    settings_path: str,
    service_name: str,
    path_of_logs_to_send='', log_path=None,
    enable_logging=False,
    tail=20,
    delay=300
):

    if log_path is not None:
        path_of_logs_to_send = log_path
        print(
            'WARNING: `log_path` is deprecated since January 2022, '
            'use `path_of_logs_to_send` instead'
        )
    start_time = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    t = threading.currentThread()
    setattr(t, "stop_thread", False)
    # This stopping mechanism is implemented according to this post:
    # https://stackoverflow.com/questions/18018033/how-to-stop-a-looping-thread-in-python

    for i in range(delay):
        if getattr(t, "stop_thread", True) is True:
            print('send_service_start_notification() stopped')
            return
        time.sleep(1)

    lines = '[NA]'
    try:
        if os.path.exists(path_of_logs_to_send):
            with open(path_of_logs_to_send, 'r') as log_file:
                lines = log_file.readlines()[-1 * tail:]
                lines = ''.join(lines)
    except Exception as e:
        err_msg = f'Unable to read log file (notification will be sent anyway): {e}'
        print(err_msg)
        logging.info(err_msg)

    mainbody = f'Service [{service_name}] started at {start_time}\n\n'
    mainbody += f'Latest log:\n{lines}'

    send_email_from_settings(
        settings_path=settings_path,
        subject=f'Service [{service_name}] started',
        mainbody=mainbody,
        fontsize=2, enable_logging=enable_logging)


def send_email_from_settings(
    settings_path: str,
    subject: str,
    mainbody: str,
    fontsize: int = 2,
    enable_logging: bool = True
):

    # No exception caught: if it fails, it fails
    with open(settings_path, 'r') as json_file:
        json_str = json_file.read()
        settings = json.loads(json_str)

    send(
        from_host=settings['email']['from_host'],
        from_port=settings['email']['from_port'],
        from_name=settings['email']['from_name'],
        from_address=settings['email']['from_address'],
        from_password=settings['email']['from_password'],
        to_name=settings['email']['to_name'],
        to_address=settings['email']['to_address'],
        subject=subject,
        mainbody=mainbody,
        fontsize=fontsize,
        enable_logging=enable_logging
    )


def send(from_host: str, from_port: int,
         from_name: str, from_address: str, from_password: str,
         to_name: str, to_address: str,
         subject: str, mainbody: str, fontsize: int=2,
         log=None, enable_logging=False):

    if log is not None:
        enable_logging = log
        print(
            'WARNING: `log` is deprecated since January 2022, '
            'use `enable_logging` instead'
        )

    mainbody = mainbody.replace('\n', '<br>')
    mainbody = mainbody.replace('\\n', '<br>')

    msg = (
        f'From: {from_name} <{from_address}>\n'
        f'To: {to_name} <{to_address}>\n'
        'Content-Type: text/html; charset="UTF-8"\n'
        f'Subject: {subject}\n'
        '<meta http-equiv="Content-Type" content="text/html charset=UTF-8" />'
        f'<html><fontsize="{fontsize}" color="black">{mainbody}</font></html>')

    # No exception caught: if it fails, it fails
    smtpObj = smtplib.SMTP(host=from_host, port=from_port)
    smtpObj.starttls()
    smtpObj.login(from_address, from_password)
    smtpObj.sendmail(from_address, to_address, msg.encode('utf-8'))
    smtpObj.quit()
    if enable_logging:
        # logging works as long as caller has configured a logger properly
        logging.info(f'Email [{subject}] sent successfully')
