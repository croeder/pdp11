'''pdp 11 emulator utilities'''
import logging

MASK_LONG     = 0o37777777777 # 32 bits
MASK_LONG_MSB = 0o20000000000
MASK_WORD     = 0o177777 # 16 bits
MASK_WORD_MSB = 0o100000
MASK_BYTE     = 0o000377 # 8 bits
MASK_BYTE_MSB = 0o000200

def oct3(word):
    """format an octal to be 6 digits wide:
    0o27 -> 027"""
    octal = oct(word)[2:]
    padding_zeroes = '0000'[0:3-len(octal)]
    result = f'{padding_zeroes}{octal}'
    return result

def oct6(word):
    """format an octal to be 6 digits wide:
    0o2127 -> 002127"""
    octal = oct(word)
    padding_zeroes = '000000'[0:8 - len(octal)]
    result = f'{octal[2:2]}{padding_zeroes}{octal[2:]}'
    return result

def pad(string, width):
    """right-pad a string to width charactcers"""
    padding = '                    '[0:width - len(string)]
    result = f'{string}{padding}'
    return result

def twosComplementNegative(source):
    """Convert positive word to 2's completemt negative word"""
    # invert bits and add 1 (then mask_word for PDP11ness)
    result = (~source + 1) & MASK_WORD
    logging.debug(f'twosComplementNegative({oct(source)}) returns {oct(result)}')
    return result

def twosComplementNegativeLong(source):
    """Convert positive word to 2's completemt negative word"""
    # invert bits and add 1 (then mask_word for PDP11ness)
    result = (~source + 1) & MASK_LONG
    logging.debug(f'twosComplementNegativeLong({oct(source)}) returns {oct(result)}')
    return result

def pythonifyPDP11Byte(source):
    """convert PDP11 byte integer which may be 2's complement negative to pythonish integer"""
    # If it's a PDP11 positive, just return that
    # If it's a PDP11 negative, then
    #    extend the bits, subtract 1, invert
    result = source & MASK_BYTE
    if (MASK_BYTE_MSB & source) == MASK_BYTE_MSB:
        logging.debug(f'MASK_BYTE_MSB & source: {oct(MASK_BYTE_MSB & source)}')
        result = -~((source | ~MASK_BYTE) - 1)
    logging.debug(f'pythonifyPDP11Byte({oct(source)}) returns {result}')
    return result

def pythonifyPDP11Word(source):
    """convert PDP11 word integer which may be 2's complement negative to pythonish integer"""
    # If it's a PDP11 positive, just return that
    # If it's a PDP11 negative, then
    #    extend the bits, subtract 1, invert
    result = source
    if (MASK_WORD_MSB & source) == MASK_WORD_MSB:
        result = -~((source | ~MASK_WORD) - 1)
    logging.debug(f'pythonifyPDP11Word({oct(source)}) returns {result}')
    return result

def pythonifyPDP11Long(source):
    """convert PDP11 longword integer which may be 2's complement negative to pythonish integer"""
    # If it's a PDP11 positive, just return that
    # If it's a PDP11 negative, then
    #    extend the bits, subtract 1, invert
    result = source
    if (MASK_LONG_MSB & source) == MASK_LONG_MSB:
        result = -~((source | ~MASK_LONG) - 1)
    logging.debug(f'pythonifyPDP11Long({oct(source)} = {source}) returns {oct(result)} = {result}')
    return result

def PDP11WordifyPythonInteger(source):
    """convert a positive or negative Pyton integer to PDP-11 word"""
    result = 0
    if source >= 0:
        result = source & MASK_WORD
    else:
        result = twosComplementNegative(-source)
    logging.debug(f'PDP11WordifyPythonInteger({source}) returns {oct(result)}')
    return result

def PDP11LongifyPythonInteger(source):
    """convert a positive or negative Pyton integer to PDP-11 Long Word"""
    result = 0
    if source >= 0:
        result = source & MASK_LONG
    else:
        result = twosComplementNegativeLong(-source)
    logging.debug(f'PDP11LongifyPythonInteger({source}) returns {oct(result)}')
    return result

def extendSign(source):
    """convert word with sign but set to python negative integer"""
    result = source
    if (MASK_WORD_MSB & source) == MASK_WORD_MSB:
        result = source & ~0
    return result

def safe_character(self, byte):
    """return character if it is printable"""
    if byte > 31:
        result = chr(byte)
    else:
        low_ascii = ['NUL', 'SOH', 'STX', 'ETX', 'EOT', 'ENQ', 'ACK', 'BEL',
                     'BS', 'HT', 'LF', 'VF', 'FF', 'CR', 'SO', 'SI',
                     'DLE', 'DC1', 'DC2', 'DC3', 'DC4', 'NAK', 'SYN', 'ETB',
                     'CAN', 'EM', 'SUB', 'ESC', 'FS', 'GS', 'RS', 'US']
        result = low_ascii[byte]
    return result
