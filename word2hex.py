
from sys import argv

def load_lines():
    with open(argv[1]) as word_file:
        valid_words = word_file.read().splitlines()

    return valid_words


def hexspeak(s):
    acceptable_chars = "abcdefgilostz "
    s = s.replace("ck", "cc")
    for char in s:
        if char not in acceptable_chars:
            return
    return s.translate(
        str.maketrans({
            "g": "6",
            "i": "1",
            "l": "1",
            "o": "0",
            "s": "5",
            "t": "7",
            "z": "2",
        })
    ).upper()

if __name__ == '__main__':
    print(hexspeak("coffee"))
