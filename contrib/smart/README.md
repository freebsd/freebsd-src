# README #

### What is this repository for? ###

* Application to output the SMART values of disks

### How do I get set up? ###

Clone this repository onto a FreeBSD box and run make

    $ git clone https://github.com/ctuffli/smart
    $ cd smart && make

or install it from ports ( http://www.freshports.org/sysutils/smart/ )

### How to use
    Usage: smart [-htxi] [-a attribute[,attribute]...] <device name>
            -h, --help
            -t, --threshold : also print out the threshold values
            -x, --hex : print the values out in hexadecimal
            -a, --attribute : print a specific attribute(s)
            -i, --info : print general device information
            -d, --decode: decode the attribute IDs
            -D, --no-decode: don't decode the attribute IDs
            -v, --version : print the version and copyright

### Example
* List the raw attributes of SATA device /dev/ada0

        smart ada0

* List the decoded attributes of NVMe device /dev/nda0

        smart -d nda0

  or

        diskhealth nda0

### What does the raw output mean?
The format and location of SMART / health data varies across protocols.
To simplify the output, the application uses a Dumb Unified Model of
SMART Buffers. In this model, SMART data is located in one or more log
pages. Each page contains one or more values ("attributes")
differentiated by an ID. Note that ID's are only unique within a log
page. Thus, the application outputs:

    <Log Page ID> <Attribute ID> <Attribute value>
for each selected attribute. Threshold values, if defined by the protocol
and selected by the user, are printed after the attribute value.

See the shell scripts `atasmart`, `nvmesmart`, and `scsismart` for examples of parsing the output.

### What is the decoded output?
Decoded output converts numeric values (Log Page, Attribute ID and Value tuples) into human-readable descriptions.

The output for protocols like NVMe and SCSI which provide standardized descriptions will match the text in the specification. ATA, however, allows vendors to define their own attributes. For ATA, descriptions come from the "SMART Attribute Descriptions (SAD)" (ANSI - INCITS TR-54) specification which documents attributes common to multiple ATA drive vendors.

### Protocol Specific Notes
* __ATA__ : The attribute and values follow the 'standard'. The log page is the Feature value used in ATA command. Thus, the default page is 208 / 0xd0 (a.k.a SMART Read Data). The threshold values printed are status flags, current value, and worst value. The SMART Return Status (Feature 218 / 0xda) indicates the reliability status of the device and is sometimes used as a top-level SMART health indication. While this command does not return data, the application encodes "no errors" as 0x0 and "threshold exceeded" as 0x1 in attribute 0.
* __NVMe__ : The Log Page is the SMART / Health Information LID value in the Get Log Page command (i.e. 0x2). The attribute ID is the byte offset within this page.
* __SCSI__ : The Log Page ID is the Page Code value in the Mode Sense command. The attribute ID is the parameter code defined by this page (e.g. 0 in the Write Error Counters log page is 'Errors corrected without substantial delay'). The values will depend on the Page Codes supported by a drive.
