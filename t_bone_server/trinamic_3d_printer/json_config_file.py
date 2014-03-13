import json

__author__ = 'marcus'
#TODO perhaps we shift it to some better place later - like /etc??
_config_directory = '/home/root/t3d/server/'
_config_file = _config_directory+'printer_config.json'


def read():
    json_config_file = open(_config_file)
    data = json.load(json_config_file)
    json_config_file.close()
    return data
