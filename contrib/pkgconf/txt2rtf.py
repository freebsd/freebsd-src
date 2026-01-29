def text_to_rtf(input_file: str, output_file: str) -> None:
    with open(input_file, "r", encoding="utf-8") as file:
        text_content = file.read()

    text_content = text_content.replace("\\", "\\\\")
    text_content = text_content.replace("{", "\\{")
    text_content = text_content.replace("}", "\\}")

    text_content = text_content.replace("\n", "\\par\n")

    rtf_content = "{\\rtf1\\ansi\\ansicpg1252\\cocoartf2580\\cocoasubrtf220\n"
    rtf_content += "{\\fonttbl\\f0\\fswiss\\fcharset0 Helvetica;}\n"
    rtf_content += "\\vieww12000\\viewh15840\\viewkind0\n"
    rtf_content += "\\pard\\tx720\\tx1440\\tx2160\\tx2880\\tx3600\\tx4320\\pardirnatural\\partightenfactor0\n"
    rtf_content += "\\f0\\fs24 "
    rtf_content += text_content
    rtf_content += "\n}"

    with open(output_file, "w", encoding="utf-8") as file:
        file.write(rtf_content)

    print(f"Conversion complete! RTF file saved as: {output_file}")


if __name__ == "__main__":
    import sys

    if len(sys.argv) != 3:
        print(f"Usage: python {sys.argv[0]} input.txt output.rtf")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    text_to_rtf(input_file, output_file)
