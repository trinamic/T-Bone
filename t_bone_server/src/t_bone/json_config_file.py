import json

__author__ = 'marcus'
#TODO perhaps we shift it to some better place later - like /etc??
_config_directory = '/etc/'
_config_file = _config_directory + 'tbone_config.json'


def read():
    json_config_file = open(_config_file)
    data = json.load(json_config_file)
    json_config_file.close()
    return data


def write(new_data):
    #test if we can jsonify the data ...
    json.dumps(new_data)
    #and then we write the file
    json_config_file = open(_config_file, 'w')
    try:
        data = json.dump(new_data, json_config_file)
    finally:
        json_config_file.close()

    return data
