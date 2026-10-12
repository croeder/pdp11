import time
import logging

class Logger():
    def __init__(self):
        logFormat = '%(asctime)s.%(msecs)03d000Z %(filename)s:%(funcName)s %(levelname)s : %(message)s'
        dateFormat = '%Y-%m-%dT%H:%M:%S'
        logPath = './pdp11.log'
        try:
            logging.basicConfig(filename=logPath, level=logging.INFO, format=logFormat,
                                datefmt=dateFormat, force=True, filemode='w')
        except OSError:
            # NEW: as in the C and Go versions, logging is off when the log
            # file can't be opened (the game's run script relies on this).
            logging.disable(logging.CRITICAL)
            return
        logging.Formatter.converter = time.gmtime
        logging.info(f"{logPath} begins")
