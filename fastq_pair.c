/*
 * Pair a fastq file using a quick index.
 */

#include "fastq_pair.h"
#include "robstr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pair_files(char *left_fn, char *right_fn, struct options *opt) {

    // our hash for the fastq ids.
    struct idloc **ids;
    ids = malloc(sizeof(*ids) * opt->tablesize);

    // if we are not able to allocate the memory for this, there is no point continuing!
    if (ids == NULL) {
        fprintf(stderr, "We cannot allocation the memory for a table size of %d. Please try a smaller value for -t\n", opt->tablesize);
        exit(-1);
    }

    //for (int i = 0; i < opt->tablesize; i++)
    //    ids[i] = NULL;

    FILE *lfp;

    char *line = malloc(sizeof(char) * MAXLINELEN + 1);

    if ((lfp = fopen(left_fn, "r")) == NULL) {
        fprintf(stderr, "Can't open file %s\n", left_fn);
        exit(1);
    }

    char *aline; /* this variable is not used, it suppresses a compiler warning */

    long int nextposition = 0;

    /*
     * Read the first file and make an index of that file.
     */
    while ((aline = fgets(line, MAXLINELEN, lfp)) != NULL) {
        struct idloc *newid;
        newid = (struct idloc *) malloc(sizeof(*newid));

        if (newid == NULL) {
            fprintf(stderr, "Can't allocate memory for new ID pointer\n");
            return 0;
        }

        line[strcspn(line, " ")] = '\0';

        /*
         * Figure out what the match mechanism is. We have three examples so
         *     i.   using /1 and /2
         *     ii.  using /f and /r
         *     iii. just having the whole name
         *
         * If there is a /1 or /2 in the file name, we set that part to null so the string is only up
         * to before the / and use that to store the location.
         */

        char lastchar = line[strlen(line)-1];
        if ('1' == lastchar || '2' == lastchar || 'f' == lastchar ||  'r' == lastchar)
            line[strlen(line)-1] = '\0';


        newid->id = dupstr(line);
        newid->pos = nextposition;
        newid->printed = false;
        newid->next = NULL;

        unsigned hashval = hash(newid->id) % opt->tablesize;
        newid->next = ids[hashval];
        ids[hashval] = newid;

        /* read the next three lines and ignore them: sequence, header, and quality */
        for (int i=0; i<3; i++)
            aline = fgets(line, MAXLINELEN, lfp);

        nextposition = ftell(lfp);
    }


    /*
     * Now just print all the id lines and their positions
     */

    if (opt->print_table_counts) {
        fprintf(stdout, "Bucket sizes\n");

        for (int i = 0; i < opt->tablesize; i++) {
            struct idloc *ptr = ids[i];
            int counter=0;
            while (ptr != NULL) {
                // fprintf(stdout, "ID: %s Position %ld\n", ptr->id, ptr->pos);
                counter++;
                ptr = ptr->next;
            }
            fprintf(stdout, "%d\t%d\n", i, counter);
        }
    }

   /* now we want to open output files for left_paired, right_paired, and right_single */

    FILE * left_paired;
    FILE * left_single;
    FILE * right_paired;
    FILE * right_single;

    char *lpfn = catstr(dupstr(left_fn), ".paired.fq");
    char *rpfn = catstr(dupstr(right_fn), ".paired.fq");
    char *lsfn = catstr(dupstr(left_fn), ".single.fq");
    char *rsfn = catstr(dupstr(right_fn), ".single.fq");

    printf("Writing the paired reads to %s and %s.\nWriting the single reads to %s and %s\n", lpfn, rpfn, lsfn, rsfn);

    if ((left_paired = fopen(lpfn, "w")) == NULL ) {
        fprintf(stderr, "Can't open file %s\n", lpfn);
        exit(1);
    }

    if ((left_single = fopen(lsfn, "w")) == NULL) {
        fprintf(stderr, "Can't open file %s\n", lsfn);
        exit(1);
    }
    if ((right_paired = fopen(rpfn, "w")) == NULL) {
        fprintf(stderr, "Can't open file %s\n", rpfn);
        exit(1);
    }

    if ((right_single = fopen(rsfn, "w")) == NULL) {
        fprintf(stderr, "Can't open file %s\n", rsfn);
        exit(1);
    }



    /*
    * Now read the second file, and print out things in common
    */

    int left_paired_counter=0;
    int right_paired_counter=0;
    int left_single_counter=0;
    int right_single_counter=0;

    FILE *rfp;

    if ((rfp = fopen(right_fn, "r")) == NULL) {
        fprintf(stderr, "Can't open file %s\n", left_fn);
        exit(1);
    }

    nextposition = 0;

    while ((aline = fgets(line, MAXLINELEN, rfp)) != NULL) {

        // make a copy of the current line so we can print it out later.
        char *headerline = dupstr(line);

        line[strcspn(line, " ")] = '\0';

        /* remove the last character, as we did above */

        char lastchar = line[strlen(line)-1];
        if ('1' == lastchar || '2' == lastchar || 'f' == lastchar ||  'r' == lastchar)
            line[strlen(line)-1] = '\0';

        // now see if we have the mate pair
        unsigned hashval = hash(line) % opt->tablesize;
        struct idloc *ptr = ids[hashval];
        long int posn = -1; // -1 is not a valid file position
        while (ptr != NULL) {
            if (strcmp(ptr->id, line) == 0) {
                posn = ptr->pos;
                ptr->printed = true;
            }
            ptr = ptr->next;
        }

        if (posn != -1) {
            // we have a match.
            // lets process the left file
            fseek(lfp, posn, SEEK_SET);
            left_paired_counter++;
            for (int i=0; i<=3; i++) {
                aline = fgets(line, MAXLINELEN, lfp);
                fprintf(left_paired, "%s", line);
            }
            // now process the right file
            fprintf(right_paired, "%s", headerline);
            right_paired_counter++;
            for (int i=0; i<=2; i++) {
                aline = fgets(line, MAXLINELEN, rfp);
                fprintf(right_paired, "%s", line);
            }
        }
        else {
            fprintf(right_single, "%s", headerline);
            right_single_counter++;
            for (int i=0; i<=2; i++) {
                aline = fgets(line, MAXLINELEN, rfp);
                fprintf(right_single, "%s", line);
            }
        }
    }

    /* all that remains is to print the unprinted singles from the left file */

    for (int i = 0; i < opt->tablesize; i++) {
        struct idloc *ptr = ids[i];
        while (ptr != NULL) {
            if (! ptr->printed) {
                fseek(lfp, ptr->pos, SEEK_SET);
                left_single_counter++;
                for (int n=0; n<=3; n++) {
                    aline = fgets(line, MAXLINELEN, lfp);
                    fprintf(left_single, "%s", line);
                }
            }
            ptr = ptr->next;
        }
    }

    fprintf(stderr, "Left paired: %d\t\tRight paired: %d\nLeft single: %d\t\tRight single: %d\n",
            left_paired_counter, right_paired_counter, left_single_counter, right_single_counter);

    fclose(lfp);
    fclose(rfp);

    free(ids);
    free(line);

    return 0;
}


unsigned hash (char *s) {
    unsigned hashval;

    for (hashval=0; *s != '\0'; s++)
        hashval = *s + 31 * hashval;
    return hashval;
}
