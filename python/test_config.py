import logging
from pdp11_logger import Logger
from pdp11_config import Config

logger = Logger()

class TestClass():
    def test_default(self):
        config = Config()
        name = config.lookup('config', 'name')
        assert name == 'default'

    def test_ram(self):
        config = Config()
        bits = config.lookup('ram', 'bits')
        assert bits == 16

    def test_barf(self):
        config = Config()
        barf = config.lookup('config', 'gook')
        assert barf == 'ERROR'

        barf = config.lookup('gobbledy', 'gook')
        assert barf == 'ERROR'
