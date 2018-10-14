
/* **********************************************************************
   * Sec-Toss.C                                                         *
   *                                                                    *
   * Copyright by Fredric L. Rice, December 1994.                       *
   *                                                                    *
   * The Skeptic Tank, 1:102/890.0, (818) 335-9601.                     *
   *                                                                    *
   ********************************************************************** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloc.h>
#include <conio.h>
#include <dir.h>
#include <time.h>
#include <ctype.h>

/* **********************************************************************
   * Any macros?                                                        *
   *                                                                    *
   ********************************************************************** */

#define The_Version     "1.00"
#define TRUE            1
#define FALSE           0
#define USHORT          unsigned short
#define skipspace(s)    while (isspace(*s))     ++s

/* **********************************************************************
   * The message file format offered here is Fido format which has      *
   * been tested with OPUS and Dutchie. It represents the latest        *
   * format that I know about.  The format, in fact, appears to be      *
   * locked-up in the past, prompting the need for kludge-lines to      *
   * handle route information not included in the mindset of the early  *
   * pioneers of FidoNet.                                               *
   *                                                                    *
   ********************************************************************** */

    struct fido_msg {
        char from[36];             /* Who the message is from             */
        char to[36];               /* Who the message to to               */
        char subject[72];          /* The subject of the message          */
        char date[20];             /* Message creation date/time          */
        USHORT times;              /* Number of time the message was read */
        USHORT destination_node;   /* Intended destination node           */
        USHORT originate_node;     /* The originator node of the message  */
        USHORT cost;               /* Cost to send this message           */
        USHORT originate_net;      /* The originator net of the message   */
        USHORT destination_net;    /* Intended destination net number     */
        USHORT destination_zone;   /* Intended zone for the message       */
        USHORT originate_zone;     /* The zone of the originating system  */
        USHORT destination_point;  /* Is there a point to destination?    */
        USHORT originate_point;    /* Point that originated the message   */
        USHORT reply;              /* Thread to previous reply            */
        USHORT attribute;          /* Message type                        */
        USHORT upwards_reply;      /* Thread to next message reply        */
    } message;                     /* Create one of this structure.       */

/* **********************************************************************
   * 'Attribute' bit definitions, some of which we will use             *
   *                                                                    *
   ********************************************************************** */

#define Fido_Private            0x0001
#define Fido_Crash              0x0002
#define Fido_Read               0x0004
#define Fido_Sent               0x0008
#define Fido_File_Attach        0x0010
#define Fido_Forward            0x0020
#define Fido_Orphan             0x0040
#define Fido_Kill               0x0080
#define Fido_Local              0x0100
#define Fido_Hold               0x0200
#define Fido_Reserved1          0x0400
#define Fido_File_Request       0x0800
#define Fido_Ret_Rec_Req        0x1000
#define Fido_Ret_Rec            0x2000
#define Fido_Req_Audit_Trail    0x4000
#define Fido_Update_Req         0x8000

/* **********************************************************************
   * Here are the errorlevels we are allowed to exit with.              *
   *                                                                    *
   ********************************************************************** */

#define No_Problem              0
#define Cant_Find_Config_File   10
#define Cant_Open_File          11
#define Configuration_Bad       12
#define Cant_Create_Message     13
#define Cant_Write_Message      14

/* **********************************************************************
   * Define local data storage                                          *
   *                                                                    *
   ********************************************************************** */

    static char config_directory[101];
    static char network_directory[101];
    static char destination_directory[101];
    static char keyword[101];
    static char want_diag;
    static USHORT highest_message_number;
    static USHORT total_messages;
    static USHORT files_moved;
    static USHORT files_examined;

/* **********************************************************************
   * Set the string offered to uppercase.                               *
   *                                                                    *
   ********************************************************************** */

void ucase(char *this_record)
{
    while (*this_record) {
        if (*this_record > 0x60 && *this_record < 0x7B) {
            *this_record = *this_record - 32;
        }

        this_record++;
    }
}

/* **********************************************************************
   * Find the highest message number and return it.                     *
   *                                                                    *
   ********************************************************************** */

static void find_highest_message_number(char *directory)
{
    char result;
    char directory_search[101];
    struct ffblk file_block;

/*
 * Start out saying we don't have any
 */

    highest_message_number = 0;
    total_messages = 0;

/*
 * Build the directory name to search for, include \ if needed
 */

    (void)strcpy(directory_search, directory);

    if (directory[strlen(directory) - 1] != '\\')
        (void)strcat(directory, "\\");

    (void)strcat(directory_search, "*.MSG");

/*
 * See if we have at least one
 */

    result = findfirst(directory_search, &file_block, 0x16);

    if (! result) {
        total_messages++;

        if (atoi(file_block.ff_name) > highest_message_number) {
            highest_message_number = atoi(file_block.ff_name);
        }
    }

/*
 * Scan all messages until we know the highest message number
 */

    while (! result) {
        result = findnext(&file_block);

        if (! result) {
            total_messages++;

            if (atoi(file_block.ff_name) > highest_message_number) {
                highest_message_number = atoi(file_block.ff_name);
            }
        }
    }
}

/* **********************************************************************
   * Extract the configuration file information.  Do it now!            *
   *                                                                    *
   ********************************************************************** */
   
static void extract_configuration(void)
{
    FILE *fin;
    char record[201], *atpoint;

/*
 * Open the file
 */

    if ((fin = fopen(config_directory, "rt")) == (FILE *)NULL) {
        textcolor(LIGHTRED);

        (void)cprintf("\r\nConfiguration file [%s] can't be found!\r\n",
            config_directory);

        fcloseall();
        exit(Cant_Find_Config_File);
    }

/*
 * Go through the file extracting all lines.
 * Then evaluate what they are.
 */

    while (! feof(fin)) {
        (void)fgets(record, 200, fin);

        if (! feof(fin)) {
            atpoint = record;
            skipspace(atpoint);
            atpoint[strlen(atpoint) - 1] = (char)NULL;

            if (! strnicmp(atpoint, "net-directory", 13)) {
                atpoint += 13;
                skipspace(atpoint);
                (void)strcpy(network_directory, atpoint);
            }
            else if (! strnicmp(atpoint, "dest-directory", 14)) {
                atpoint += 14;
                skipspace(atpoint);
                (void)strcpy(destination_directory, atpoint);
            }
            else if (! strnicmp(atpoint, "keyword", 7)) {
                atpoint += 7;
                skipspace(atpoint);
                ucase(atpoint);
                (void)strcpy(keyword, atpoint);
            }
        }
    }

/*
 * Close the file
 */

    (void)fclose(fin);

/*
 * Make sure that we got everything we need to run
 */

    if (network_directory[0] == (char)NULL) {
        textcolor(LIGHTRED);

        (void)cprintf("\r\nThe configuration file [%s] doesn't have\r\n",
            config_directory);

        (void)cprintf("'net-directory' defined anywhere in it!\r\n");

        fcloseall();
        exit(Configuration_Bad);
    }

    if (destination_directory[0] == (char)NULL) {
        textcolor(LIGHTRED);

        (void)cprintf("\r\nThe configuration file [%s] doesn't have\r\n",
            config_directory);

        (void)cprintf("'dest-directory' defined anywhere in it!\r\n");

        fcloseall();
        exit(Configuration_Bad);
    }

    if (keyword[0] == (char)NULL) {
        textcolor(LIGHTRED);

        (void)cprintf("\r\nThe configuration file [%s] doesn't have\r\n",
            config_directory);

        (void)cprintf("'keyword' defined anywhere in it!\r\n");

        fcloseall();
        exit(Configuration_Bad);
    }
}

/* **********************************************************************
   * Initialize this.                                                   *
   *                                                                    *
   ********************************************************************** */
   
static void initialize(void)
{
    USHORT loop;
    char *env;

/*
 * Get our environment variable to determine the path
 * to our configuration file and error message file
 */

    if (NULL == (env = getenv("SEC-TOSS"))) {
        (void)strcpy(config_directory, "SEC-TOSS.CFG");
    }
    else {
        (void)strcpy(config_directory, env);

        if (config_directory[strlen(config_directory) - 1] != '\\') {
            (void)strcat(config_directory, "\\");
        }

        (void)strcat(config_directory, "SEC-TOSS.CFG");
    }                                                      

/*
 * Initialize local data storage
 */

    want_diag = FALSE;
    network_directory[0] = (char)NULL;
    destination_directory[0] = (char)NULL;
    keyword[0] = (char)NULL;
    files_moved = 0;
    files_examined = 0;
    total_messages = 0;
}

/* **********************************************************************
   * Say hello.                                                         *
   *                                                                    *
   ********************************************************************** */

static void say_hello(void)
{
    clrscr();
    gotoxy(20, 2);
    textcolor(LIGHTGREEN);
    (void)cprintf("SEC-Toss Ver %s (%s)", The_Version, __DATE__);
    gotoxy(1, 4);
    textcolor(LIGHTBLUE);
    (void)cprintf("Network directory: [%s]", network_directory);
    gotoxy(1, 5);
    textcolor(LIGHTBLUE);

    (void)cprintf("Destination directory: [%s] (%d messages)",
        destination_directory, total_messages);

    gotoxy(1, 6);
    textcolor(YELLOW);
    (void)cprintf("Search keyword(s): [%s]", keyword);
    gotoxy(1, 8);
    textcolor(LIGHTGREEN);
}

/* **********************************************************************
   * Move the message                                                   *
   *                                                                    *
   ********************************************************************** */

static void toss_message(char *from)
{
    char full_file[101];
    char file_name[101];
    char record[101];
    FILE *fin;
    FILE *fout;

/*
 * Build the destination file name to create, including \ if it's needed
 */

    (void)sprintf(file_name, "%d.MSG", ++highest_message_number);

	(void)strcpy(full_file, destination_directory);

    if (full_file[strlen(full_file) - 1] != '\\')
        (void)strcat(full_file, "\\");

    (void)strcat(full_file, file_name);

/*
 * Create the destination file
 */

    if ((fout = fopen(full_file, "wt")) == (FILE *)NULL) {
        textcolor(LIGHTRED);
        (void)cprintf("\r\nI couldn't create file [%s]!\n", full_file);
        fcloseall();
        exit(Cant_Create_Message);
    }

/*
 * Open the source file
 */

    if ((fin = fopen(from, "rt")) == (FILE *)NULL) {
        textcolor(LIGHTRED);
        (void)cprintf("\r\nI couldn't open file [%s]!\n", from);
        fcloseall();
        exit(Cant_Open_File);
    }

/*
 * Read the source header and write to the destination
 */

    (void)fread(&message, sizeof(message), 1, fin);
    (void)fwrite(&message, sizeof(message), 1, fout);

/*
 * Go through the file and copy the text
 */

    while (! feof(fin)) {
        (void)fgets(record, 100, fin);

        if (! feof(fin)) {
            (void)fputs(record, fout);
        }
    }

/*
 * Close both files
 */

    (void)fclose(fin);
    (void)fclose(fout);

/*
 * Delete the original
 */
     (void)unlink(from);

/*
 * Incriment the moved count
 */

    files_moved++;
}

/* **********************************************************************
   * See if the file we're being pointed toward contains the keyword    *
   * we're interested in.                                               *
   *                                                                    *
   * If it does, move it, otherwise leave it alone.                     *
   *                                                                    *
   ********************************************************************** */

static void process_message(char *file_name)
{
    char full_file[101];
    char record[101];
    FILE *fin;

/*
 * Build the file name to open, including \ if it's needed
 */

    (void)strcpy(full_file, network_directory);

    if (full_file[strlen(full_file) - 1] != '\\')
        (void)strcat(full_file, "\\");

    (void)strcat(full_file, file_name);

/*
 * Open the file
 */

    if ((fin = fopen(full_file, "rt")) == (FILE *)NULL) {
        textcolor(LIGHTRED);
        (void)cprintf("\r\nI couldn't open file [%s]!\n", full_file);
        fcloseall();
        exit(Cant_Open_File);
    }

/*
 * Incriment the number of files examined
 */

    files_examined++;

/*
 * Get the header and then ignore it
 */

    (void)fread(&message, sizeof(message), 1, fin);

/*
 * Go through the file and search for the keyword.
 * If the word is found, toss the message.
 */

    while (! feof(fin)) {
        (void)fgets(record, 100, fin);

        if (! feof(fin)) {
            ucase(record);

            if (strstr(record, keyword) != (char *)NULL) {
                (void)fclose(fin);
				toss_message(full_file);
                return;
            }
        }
    }

/*
 * Nope.  Don't touch it.
 */

    (void)fclose(fin);
}

/* **********************************************************************
   * Process the network mail directory                                 *
   *                                                                    *
   ********************************************************************** */

static void process_network_directory(void)
{
    char result;
    char directory_search[101];
    struct ffblk file_block;

/*
 * Build the directory name to search for, include \ if needed
 */

    (void)strcpy(directory_search, network_directory);

    if (network_directory[strlen(network_directory) - 1] != '\\')
        (void)strcat(network_directory, "\\");

    (void)strcat(directory_search, "*.MSG");

/*
 * See if we have at least one
 */

    result = findfirst(directory_search, &file_block, 0x16);

    if (! result) {
        process_message(file_block.ff_name);
    }

/*
 * Scan all messages until we find the last
 */

    while (! result) {
        result = findnext(&file_block);

        if (! result) {
            process_message(file_block.ff_name);
        }
    }
}

/* **********************************************************************
   * Offer a final report.                                              *
   *                                                                    *
   ********************************************************************** */

static void final_report(void)
{
    (void)cprintf("\r\nTotal files examined: %d\r\n", files_examined);
    (void)cprintf("\r\nTotal files moved: %d\r\n", files_moved);
}

/* **********************************************************************
   * Main entry point.                                                  *
   *                                                                    *
   ********************************************************************** */
   
void main(USHORT argc, char *argv[])
{
    USHORT loop;

/*
 * See if we want diagnostics
 */

    for (loop = 1; loop < argc; loop++) {
        if (! strnicmp(argv[loop], "/diag", 5)) {
            want_diag = TRUE;
        }
    }

/*
 * Clear the screen and launch
 */

    if (! want_diag) {
        clrscr();
    }

/*
 * Initialize us
 */

    initialize();

/*
 * Get our configuration
 */

    extract_configuration();

/*
 * Find out what the highest message number is in the
 * destination directory.  This is because we will be
 * creating messages in that directory and don't wish
 * to overwrite what's already their.
 */

    find_highest_message_number(destination_directory);

/*
 * Tell the world that we exist
 */

    say_hello();

/*
 * Process the network directory
 */

    process_network_directory();

/*
 * Report what we did
 */

    final_report();

/*
 * Close up shop
 */

    fcloseall();

/*
 * Terminate with a no-problem errorlevel value
 */

    exit(No_Problem);
}


