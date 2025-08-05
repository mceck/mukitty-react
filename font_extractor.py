#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
from PIL import Image, ImageDraw, ImageFont
import sys
import os

# Un dizionario per dare nomi più leggibili ai caratteri di controllo nei commenti C
CONTROL_CHAR_NAMES = {
    0: "NUL",
    1: "SOH",
    2: "STX",
    3: "ETX",
    4: "EOT",
    5: "ENQ",
    6: "ACK",
    7: "BEL",
    8: "BS",
    9: "HT",
    10: "LF",
    11: "VT",
    12: "FF",
    13: "CR",
    14: "SO",
    15: "SI",
    16: "DLE",
    17: "DC1",
    18: "DC2",
    19: "DC3",
    20: "DC4",
    21: "NAK",
    22: "SYN",
    23: "ETB",
    24: "CAN",
    25: "EM",
    26: "SUB",
    27: "ESC",
    28: "FS",
    29: "GS",
    30: "RS",
    31: "US",
    127: "DEL",
}


def get_char_name(code):
    """Restituisce una rappresentazione stringa del carattere per i commenti."""
    if code in CONTROL_CHAR_NAMES:
        return CONTROL_CHAR_NAMES[code]
    if 32 <= code <= 126:
        return chr(code)
    return "???"  # Per caratteri non stampabili/sconosciuti


def generate_font_data(font_path, font_size, y_offset):
    """
    Renderizza ogni carattere da 0 a 255 in una bitmap 8x8.

    Args:
        font_path (str): Percorso del file .ttf.
        font_size (int): La dimensione del font in punti da usare per il rendering.
        y_offset (int): Offset verticale per centrare meglio il carattere.

    Returns:
        dict: Un dizionario che mappa i codici dei caratteri ai loro dati bitmap.
    """
    try:
        font = ImageFont.truetype(font_path, size=font_size)
    except IOError:
        print(
            f"Errore: Impossibile trovare o caricare il font da '{font_path}'",
            file=sys.stderr,
        )
        sys.exit(1)

    all_char_data = {}

    for char_code in range(256):
        # Crea una nuova immagine 8x8 in bianco e nero (1-bit per pixel)
        # Il colore 0 è nero (pixel spento), 1 è bianco (pixel acceso)
        image = Image.new("1", (8, 8), color=0)
        draw = ImageDraw.Draw(image)

        character = chr(char_code)

        # Disegna il carattere sull'immagine.
        # L'offset (0, y_offset) è cruciale per allineare verticalmente il font.
        # Potrebbe essere necessario sperimentare con y_offset e font_size.
        draw.text((0, y_offset), character, font=font, fill=1)

        # Estrai i dati dei pixel e convertili in byte
        byte_rows = []
        for y in range(8):
            current_byte = 0
            for x in range(8):
                pixel_is_on = image.getpixel((x, y))
                if pixel_is_on:
                    # L'MSB (Most Significant Bit) corrisponde al pixel più a sinistra (x=0)
                    current_byte |= 1 << (7 - x)
            byte_rows.append(current_byte)

        all_char_data[char_code] = byte_rows

    return all_char_data


def write_header_file(output_path, font_data, array_name):
    """
    Scrive i dati del font in un file header C.

    Args:
        output_path (str): Percorso del file .h di destinazione.
        font_data (dict): Dati bitmap generati.
        array_name (str): Nome da usare per l'array C.
    """
    with open(output_path, "w", encoding="utf-8") as f:
        f.write("#include <stdint.h>\n\n")
        f.write(
            "/* Simple 8x8 bitmap font - each character is 8 bytes, each bit represents a pixel */\n"
        )
        f.write(f"static const uint8_t {array_name}[256][8] = {{\n")

        # Scrivi i dati per ogni carattere
        for code in sorted(font_data.keys()):
            hex_values = ", ".join([f"0x{byte:02X}" for byte in font_data[code]])
            char_name = get_char_name(code)

            # Aggiungi commenti di raggruppamento per leggibilità
            if code == 32:
                f.write("    /* ASCII 32-64 (Simboli e numeri) */\n")
            elif code == 65:
                f.write("    /* ASCII 65-90 (A-Z) */\n")
            elif code == 91:
                f.write("    /* ASCII 91-96 (Simboli) */\n")
            elif code == 97:
                f.write("    /* ASCII 97-122 (a-z) */\n")
            elif code == 123:
                f.write("    /* ASCII 123-127 (Simboli e DEL) */\n")
            elif code == 128:
                f.write("    /* Extended ASCII */\n")

            # Formatta la riga per l'inizializzatore C
            # Esempio: [32] = {0x00, ...}, /* ASCII 32 ( ) */
            f.write(
                f"    [{code:<3}] = {{{hex_values}}}, /* ASCII {code} ({char_name}) */\n"
            )

        f.write("};\n")
    print(f"Successo! File header generato in: '{output_path}'")


def main():
    parser = argparse.ArgumentParser(
        description="Converte un file font .ttf in un header C con una bitmap 8x8.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "font_file",
        help="Percorso del file font di input (.ttf o .otf).",
    )
    parser.add_argument(
        "--output-file",
        help="Percorso del file header C di output (.h).",
        default="font.h",
    )
    parser.add_argument(
        "--font-size",
        type=int,
        default=11,
        help="Dimensione in punti del font per il rendering (potrebbe essere necessario sperimentare).\nDefault: 9",
    )
    parser.add_argument(
        "--y-offset",
        type=int,
        default=-3,
        help="Offset verticale in pixel per allineare il font.\nValori negativi lo spostano in alto. Default: -2",
    )
    parser.add_argument(
        "--array-name",
        type=str,
        default="font_8x8",
        help="Nome per l'array nel file C.\nDefault: font_8x8",
    )

    args = parser.parse_args()

    # Controlla che il file di input esista
    if not os.path.exists(args.font_file):
        print(
            f"Errore: Il file di input '{args.font_file}' non è stato trovato.",
            file=sys.stderr,
        )
        sys.exit(1)

    font_data = generate_font_data(args.font_file, args.font_size, args.y_offset)
    write_header_file(args.output_file, font_data, args.array_name)


if __name__ == "__main__":
    main()
