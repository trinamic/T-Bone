from flask import Flask, render_template
import logging
import helpers
import json_config_file

app = Flask(__name__)
_logger = logging.getLogger(__name__)
#this is THE printer - just a dictionary with anything
_printer = None


@app.route('/')
def hello_world():
    template_dictionary = templating_defaults()
    return render_template("index.html", **template_dictionary)


def templating_defaults():
    templating_dictionary = {
        'short_name': 'T-Bone',
        'full_name': 'T-Bone CNC Server'
    }
    if _printer:
        templating_dictionary['print'] = _printer.printing
        if _printer.printing:
            templating_dictionary['print_status'] = 'Printing'
        else:
            templating_dictionary['print_status'] = 'Idle'
        if _printer.machine.machine_connection:
            connection = _printer.machine.machine_connection
            templating_dictionary['queue_length'] = connection.internal_queue_length
            templating_dictionary['max_queue_length'] = connection.internal_queue_max_length
    return templating_dictionary


if __name__ == '__main__':
    #configure the overall logging
    logging.basicConfig(filename='print.log', level=logging.INFO,
                        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    logging.info('Starting print server')
    #somehow we can get several initializations - hence we store a global printer
    try:
        if not _printer:
            _printer = helpers.create_printer()
            config = json_config_file.read()
            _printer.connect()
            _printer.configure(config)
        logging.info('configured, starting web interface')
        app.run(
            host='0.0.0.0',
            debug=True,
            use_reloader=False
        )
    finally:
        #reset the printer
        _printer = None
    logging.info('Quitting print server')

