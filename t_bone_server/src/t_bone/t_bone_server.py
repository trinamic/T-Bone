import logging
import os
import threading

from flask import Flask, render_template, request
import flask
from werkzeug.utils import secure_filename
import beaglebone_helpers
from gcode_interpreter import GCodePrintThread
from t_bone import json_config_file


_logger = logging.getLogger(__name__)
#this is THE printer - just a dictionary with anything
_printer = None
_print_thread = None
_printer_busy = False
_printer_busy_lock = threading.RLock()
app = Flask(__name__,
            static_folder='static',
            static_url_path='')
UPLOAD_FOLDER = '/var/print_uploads'
MAX_CONTENT_LENGTH = 2 * 1024 * 1024 * 1024


def busy_function(original_function):
    def busy_decorator(*args, **kargs):
        global _printer_busy
        try:
            with _printer_busy_lock:
                _printer_busy = True
            result = original_function(*args, **kargs)
            return result
        finally:
            with _printer_busy_lock:
                _printer_busy = False

    return busy_decorator


@app.route('/')
def start_page():
    template_dictionary = templating_defaults()
    return render_template("index.html", **template_dictionary)


@app.route('/print', methods=['GET', 'POST'])
def print_page():
    if not _printer:
        return "there is no printer", 400
    if request.method == 'POST':
        if request.files and 'printfile' in request.files:
            file = request.files['printfile']
            if file and beaglebone_helpers.allowed_file(file.filename):
                filename = secure_filename(file.filename)
                upload_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
                try:
                    _logger.info("Saving file %s to %s", filename, upload_path)
                    file.save(upload_path)
                    _printer.prepared_file = upload_path
                except:
                    _logger.warn("unable to save file %s to %s", filename, upload_path)
        else:
            if _printer.prepared_file:
                if request.form and 'really' in request.form:
                    _logger.info("Printing %s", _printer.prepared_file)
                    global _print_thread
                    _print_thread = GCodePrintThread(_printer.prepared_file, _printer, None)
                    _print_thread.start()
                else:
                    try:
                        if os.path.exists(_printer.prepared_file):
                            os.remove(_printer.prepared_file)
                    except:
                        _logger.warn("unable to delete %s", _printer.prepared_file)
                    _printer.prepared_file = None

    template_dictionary = templating_defaults()
    if _printer.prepared_file:
        template_dictionary['print_file'] = _printer.prepared_file.rsplit('/', 1)[1]
    return render_template("print.html", **template_dictionary)


@app.route('/home/<axis>')
@busy_function
def home_axis(axis):
    if not _printer:
        return "there is no printer", 400
    all_axis = list(_printer.axis)
    if 'all' == axis:
        _printer.home(all_axis)
    elif axis in all_axis:
        _printer.home(axis)
    else:
        return "unkown axis", 400

    return "ok"


def templating_defaults():
    templating_dictionary = {
        'short_name': 'T-Bone',
        'full_name': 'T-Bone Reprap'
    }
    if _printer:
        templating_dictionary['print'] = _printer.printing
        templating_dictionary['axis'] = _printer.axis
        templating_dictionary['axis_names'] = _printer.axis_names()
        if _printer.printing:
            templating_dictionary['print_status'] = 'Printing'
        else:
            templating_dictionary['print_status'] = 'Idle'
        if _printer.machine.machine_connection:
            connection = _printer.machine.machine_connection
            templating_dictionary['queue_length'] = connection.internal_queue_length
            templating_dictionary['max_queue_length'] = connection.internal_queue_max_length
            templating_dictionary['queue_percentage'] = int(
                float(connection.internal_queue_length) / float(connection.internal_queue_max_length) * 10.0)
    return templating_dictionary


@app.route('/status')
def status():
    connection = _printer.machine.machine_connection
    base_status = {'printing': _printer.printing,
                   'busy': (_printer_busy | _printer.printing),
                   'queue_length': connection.internal_queue_length,
                   'max_queue_length': connection.internal_queue_max_length,
                   'queue_percentage': int(
                       float(connection.internal_queue_length) / float(connection.internal_queue_max_length) * 100.0)}
    if _printer.printing:
        base_status['print_status'] = 'Printing'
    else:
        base_status['print_status'] = 'Idle'

    global _print_thread
    if _print_thread and _print_thread.printing:
        base_status['lines_to_print'] = _print_thread.lines_to_print
        base_status['lines_printed'] = _print_thread.lines_printed
        base_status['lines_printed_percent'] = float(_print_thread.lines_printed) / float(
            _print_thread.lines_to_print) * 100
    return flask.jsonify(
        base_status
    )
    pass


if __name__ == '__main__':
    #configure the overall logging
    logging.basicConfig(filename='print.log', level=logging.INFO,
                        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    logging.info('Starting print server')
    #somehow we can get several initializations - hence we store a global printer
    try:
        if not os.path.exists(UPLOAD_FOLDER):
            os.mkdir(UPLOAD_FOLDER)
        if not _printer:
            _printer = beaglebone_helpers.create_printer()
            config = json_config_file.read()
            _printer.connect()
            _printer.configure(config)
            _printer.prepared_file = None
        logging.info('configured, starting web interface')
        #this has to be configured somewhere (json??)
        app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
        app.config['MAX_CONTENT_LENGTH'] = MAX_CONTENT_LENGTH
        app.run(
            host='0.0.0.0',
            debug=True,
            use_reloader=False
        )
    finally:
        #reset the printer
        _printer = None
    logging.info('Quitting print server')

