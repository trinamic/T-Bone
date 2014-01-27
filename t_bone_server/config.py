import json

__author__ = 'marcus'
_config_file = 'printer-config.json-not'


def read():
    json_config_file = open(_config_file)
    data = json.load(json_config_file)
    json_config_file.close()
    return data
