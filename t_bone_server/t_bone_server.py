from flask import Flask, render_template
import logging

app = Flask(__name__)
_logger = logging.getLogger(__name__)
#this is THE printer - just a dictionary with anything
_printer = None

@app.route('/')
def hello_world():
    template_dictionary = templating_defaults()
    return render_template("index.html", ** template_dictionary)


def templating_defaults():
    return {
        'name': 'T-Bone',
    }


if __name__ == '__main__':
    app.run(
        host='0.0.0.0',
        debug=True
    )
