
orcus-ods
=========

Usage
-----

**orcus-ods [options] FILE**

The FILE must specify a path to an existing file.

Allowed options
---------------

**-h [ --help ]**
   Print this help.

**-d [ --debug ]**
   Turn on a debug mode to generate run-time debug output.

**--dump-check**
   Dump the content to stdout in a special format used for content verification
   in automated tests.

**-o [ --output ] arg**
   Output directory path, or output file when --dump-check option is used.

**-f [ --output-format ] arg**
   Specify the format of output file.  Supported format types are:

   - csv - CSV format
   - flat - flat text format
   - html - HTML format
   - json - JSON format
   - none - no output

**--row-size arg**
   Specify the number of maximum rows in each sheet.

