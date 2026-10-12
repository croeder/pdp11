"""configuration"""
import json
import logging
from pdp11_logger import Logger

# reads a configuration file
# the name is given in the init cparameters
# if no name is given, read config_default.json
# hardware binds hardware type, base address, options, and name

# needs to set up logging level and format and stuff

# pdp11 needs to define the memory size and IO page
# (Maybe pdp11 needs to load additional instructions.)
# DL11 needs to set up a DL11 with a name and set its base address
# TV52 needs to connect to a specific DL11 by name
# Tek4010 needs to connect to a specific DL11 by name
# Disk drives need to define their symbolic names and base addresses.

class Config:
    def __init__(self, filename='config_default.json'):
        logger = Logger()
        logging.info(f'configuration file: {filename}')
        with open(filename, 'r', encoding='utf-8') as configFile:
            try:
                self.automation_config = json.load(configFile)
            except json.decoder.JSONDecodeError:
                # FIX: the Python named an undefined variable here (NameError).
                logging.error(f'There was an error while reading {filename}')

    def safe_dictionary_lookup(self, dictionry, key, default=''):
        """safely get value out of dictionary where key might be missing

        parameters:
            dictionry: list, the case parameter handed in to a parametrized method
            key: string name of value you want
            default: (optional) value to return if symbol was not set

        returns:
            the value from the dictionary, or default
        """
        try:
            item = str(dictionry[key])
        except KeyError:
            item = default
        return item

    def lookup(self, device, parameter):
        """safely get value out of configuration

        parameters:
            key1: device name
            key2: device parameter

        returns:
            the value from the dictionary, or ERROR
        """
        try:
            deviceParams = self.automation_config[device]
            value = deviceParams[parameter]
        except KeyError:
            value = "ERROR"
            logging.error(f'could not find value for {device}:{parameter}')
        return value
