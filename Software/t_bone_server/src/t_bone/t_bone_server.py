from dircache import listdir
import fnmatch
from genericpath import isfile
import json
import logging
import os
import threading

from flask import Flask, render_template, request, redirect
import flask
from werkzeug.utils import secure_filename
import beaglebone_helpers
from gcode_interpreter import GCodePrintThread
from t_bone import json_config_file

T_BONE_LOG_FILE = '/var/log/t_bone.log'

_logger = logging.getLogger(__name__)
# this is THE printer - just a dictionary with anything
_printer = None
_print_thread = None
_printer_busy = False
_printer_busy_lock = threading.RLock()
app = Flask(__name__,
            static_folder='static',
            static_url_path='')
UPLOAD_FOLDER = '/var/print_uploads'
MAX_CONTENT_LENGTH = 2 * 1024 * 1024 * 1024

axis_directions = {
    'x': {
        '+': 'right',
        '-': 'left',
    },
    'y': {
        '+': 'forward',
        '-': 'backward',
    },
    'z': {
        '+': 'down',
        '-': 'up',
    },
}


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
        if request.files and 'uploadfile' in request.files:
            file = request.files['uploadfile']
            if file and beaglebone_helpers.allowed_file(file.filename):
                filename = secure_filename(file.filename)
                upload_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
                try:
                    _logger.info("Saving file %s to %s", filename, upload_path)
                    file.save(upload_path)
                except:
                    _logger.warn("unable to save file %s to %s", filename, upload_path)
        elif 'printfile' in request.form:
            filename = request.form['printfile']
            file_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
            if isfile(file_path) and beaglebone_helpers.allowed_file(filename):
                _printer.prepared_file = file_path
                _logger.info("Printing %s", _printer.prepared_file)
                global _print_thread
                _print_thread = GCodePrintThread(_printer.prepared_file, _printer, None)
                _print_thread.start()

    template_dictionary = templating_defaults()
    files = [f for f in listdir(app.config['UPLOAD_FOLDER'])
             if isfile(app.config['UPLOAD_FOLDER'] + "/" + f) and fnmatch.fnmatch(f, '*.gcode')]
    #todo would like to http://stackoverflow.com/questions/6591931/getting-file-size-in-python
    #http://stackoverflow.com/questions/1094841/reusable-library-to-get-human-readable-version-of-file-size
    #http://stackoverflow.com/questions/237079/how-to-get-file-creation-modification-date-times-in-python
    template_dictionary['files'] = files
    if _printer.prepared_file:
        template_dictionary['print_file'] = _printer.prepared_file.rsplit('/', 1)[1]
    return render_template("print.html", **template_dictionary)


@app.route('/control', methods=['GET', 'POST'])
def control():
    if request.method == 'POST':
        if 'set-extruder-temp' in request.form:
            new_temp_ = request.form['set-extruder-temp']
            try:
                _printer.extruder_heater.set_temperature(float(new_temp_))
            except:
                _logger.error("unable to set temperature to %s", new_temp_)
        if 'set-bed-temp' in request.form:
            if _printer.heated_bed:
                new_temp_ = request.form['set-bed-temp']
                try:
                    _printer.heated_bed.set_temperature(float(new_temp_))
                except:
                    _logger.error("unable to set temperature to %s", new_temp_)
    template_dictionary = templating_defaults()
    template_dictionary['axis_directions'] = axis_directions
    return render_template("move.html", **template_dictionary)


@app.route('/move/<axis>/<amount>')
def move_axis(axis, amount):
    _printer.relative_move_to(
        {
            str(axis): float(amount)
        }
    )
    return "ok"


@app.route('/config', methods=['GET', 'POST'])
def config():
    error = None
    #first of all let's check if we have to save a changed config
    if request.method == 'POST':
        if 'config_content' in request.form:
            try:
                data = json.loads(request.form['config_content'])
                json_config_file.write(data)
            except Exception as e:
                error = e.message
    template_dictionary = templating_defaults()
    if error:
        template_dictionary['config_write_error'] = error
    config = json_config_file.read()
    #add pretty printe config to dict ...
    template_dictionary['config_content'] = json.dumps(config, indent=4, separators=(',', ': '))

    return render_template("config.html", **template_dictionary)


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
            templating_dictionary['axis_status'] = _printer.read_axis_status()
        templating_dictionary['extruder_temperature'] = "%0.1f" % _printer.extruder_heater.temperature
        templating_dictionary['extruder_set_temperature'] = "%0.1f" % _printer.extruder_heater.get_set_temperature()
        if _printer.heated_bed:
            templating_dictionary['heated_bed'] = True
            templating_dictionary['bed_temperature'] = "%0.1f" % _printer.heated_bed.temperature
            templating_dictionary['bed_set_temperature'] = "%0.1f" % _printer.heated_bed.get_set_temperature()
        else:
            templating_dictionary['heated_bed'] = False
    return templating_dictionary


@app.route('/status')
def status():
    connection = _printer.machine.machine_connection
    base_status = {'printing': _printer.printing,
                   'busy': (_printer_busy | _printer.printing),
                   'queue_length': connection.internal_queue_length,
                   'max_queue_length': connection.internal_queue_max_length,
                   'queue_percentage': int(
                       float(connection.internal_queue_length) / float(connection.internal_queue_max_length) * 100.0),
                   'extruder_temperature': "%0.1f" % _printer.extruder_heater.temperature,
                   'extruder_set_temperature': "%0.1f" % _printer.extruder_heater.get_set_temperature(),
                   'axis_status': _printer.read_axis_status()
    }
    if _printer.heated_bed:
        base_status['bed_temperature'] = "%0.1f" % _printer.heated_bed.temperature
        base_status['bed_set_temperature'] = "%0.1f" % _printer.heated_bed.get_set_temperature()
    if _printer.printing:
        base_status['print_status'] = 'Printing'
    else:
        base_status['print_status'] = 'Idle'

    if _printer.heated_bed:
        base_status['bed-temperature'] = "%0.1f" % _printer.heated_bed.temperature
        base_status['bed-set-temperature'] = "%0.1f" % _printer.heated_bed.get_set_temperature()


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


@app.route('/restart')
def restart_printer():
    if _printer:
        _printer.stop()
    create_printer()
    return redirect("/", 302)


def create_printer():
    global _printer, config
    _printer = beaglebone_helpers.create_printer()
    _printer.prepared_file = None

    config = json_config_file.read()
    _printer.connect()
    _printer.configure(config)


if __name__ == '__main__':
    #configure the overall logging
    logging.basicConfig(filename=T_BONE_LOG_FILE, level=logging.DEBUG,
                        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    #Comment following lines in order to enable logging (/var/log/t_bone.log)
    logging.disable(logging.DEBUG)
    logging.disable(logging.INFO)

    logging.info('Starting print server')
    #somehow we can get several initializations - hence we store a global printer
    try:
        if not os.path.exists(UPLOAD_FOLDER):
            os.mkdir(UPLOAD_FOLDER)
        if not _printer:
            create_printer()
        logging.info('configured, starting web interface')
        #this has to be configured somewhere (json??)
        app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
        app.config['MAX_CONTENT_LENGTH'] = MAX_CONTENT_LENGTH
        app.run(
            host='0.0.0.0',
	    port=80,
            debug=True,
            use_reloader=False
        )
    except KeyboardInterrupt:
        _printer.stop()
    finally:
        #reset the printer
        _printer = None
    logging.info('Quitting print server')

