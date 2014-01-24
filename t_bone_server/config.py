import json

__author__ = 'marcus'

def read(_config_file):
    if not _config_file:
        raise Exception("No config w/o filename!")
    json_config_file = open(_config_file)
    data = json.load(json_config_file)
    json_config_file.close()
    return data
