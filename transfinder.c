/* translatable text finder
 * 
 * recursively scans a tree for sentences that look like English and aren't in a trans() or _() function call
 * currently only supports js and html
 * 
 * current bug:
 * line 421
 * datum_hovered = Math.floor(e.offsetX/width*30);
 * parser thinks / in line above is start of regex
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/types.h>
#include "dirent.h"
#include "transfinder.h"

char shift_buf[] = "       "; /* used for checking last x chars - does not "need" to be global */
int number_of_files_found = 0; /* remains global to recurse_file otherwise we have a recursive tree issue */
int number_of_trans_needed_found = 0;

int main(int argc, char *argv[]) {
    
	char * file_name;
    
    if(argc!=2) {
		printf("usage: transfinder FILE_OR_FOLDER_NAME\n");
		return 1;
	}
	file_name=argv[1];
	if (recurse_file(file_name)) return 1;
    printf("\nEnd of Program. Found %d lines that may need translation\n", number_of_trans_needed_found);
	return 0;
}

/* if it's a file, parse it. If it's a folder, parse all descendant files */
int recurse_file(char * file_name) {
	struct stat stbuf;
    char new_file_name[MAX_PATH_LEN];
    int calced_path_len;
    
	if(stat(file_name, &stbuf) == -1) {
		printf("FNF");
		return 1;
	}
	if((stbuf.st_mode & S_IFMT) == S_IFDIR) {
        DIR *dfd; // struct for dir in general
        struct dirent *dp; // pointer to files in dir
        if ((dfd = opendir(file_name)) == NULL ) {
            printf("error opening directory '%s'\n", file_name);
            return 1;
        }
        while ((dp = readdir(dfd)) != NULL) {
            if(*(dp->d_name) == '.') continue;
/*             printf("dpname %s\n", dp->d_name); */
            calced_path_len = strlen(file_name)+strlen(dp->d_name)+2;
            if(calced_path_len > MAX_PATH_LEN) {
                printf("file path (%d) too long %s/%s\n", calced_path_len, file_name, dp->d_name);
                return 1;
            }
            sprintf(new_file_name, "%s/%s", file_name, dp->d_name);
/*           printf("about to scan %s\n", new_file_name);*/
            if(recurse_file(new_file_name)) // returns not 0
                return 1;
        }
	} else {
/*         printf("f: %s\n", file_name);*/
		if (parse_file(file_name)) return 1;
        if(++number_of_files_found>=MAX_FILES_TO_FIND ) {
            printf("hit files found cap %d\n",number_of_files_found );
            return 1;
        }
	}
    return 0;
}

/* parses file looking for translatable strings - passes those to check_and_print_phrase for further checking*/
int parse_file(char * file_name) {

    FILE *fp;
/*     printf("checking ftype: %s\n", file_name);*/
	enum known_file_types current_file_type = get_file_type(file_name); /* only uses file name*/
    if(VERBOSE) print_file_type(current_file_type);
/*     printf("done checking file type\n");*/
    if(current_file_type==unknown_file_type ) {
        return 0;
    }

    if (VERBOSE) printf("opening %s\n", file_name);
	fp = fopen(file_name, "r");

	char sentence_buffer[SENTENCE_BUF_SIZE];
	char *sentence_buf_ptr = sentence_buffer;
	int sentence_buffer_len = 0;
	int line_number = 1;
	/* int mode_stack[10] = {outside_all};*/
    int script_coming = false;
    int css_coming = false;
    int record_mode = 0; /* used inside quotes */

	int cur_char, char_no;
	enum parse_read_modes read_mode = outside_all;
	enum js_trans_func_levels trans_level = waiting_for_underscore;

	if(current_file_type == html_file ) {
        int in_block_trans = false;
		for(char_no=0;char_no<MAX_FILE_LENGTH;char_no++) {
			cur_char = getc(fp);
			if (cur_char == EOF) {
/* 				printf("EOF.\n");*/
				break;
			}
			if( PRINT_SYNTAX ) print_color_char(cur_char, read_mode);
            if(PRINT_READ_MODE) print_read_mode(read_mode);
			if (cur_char == '\n') line_number++;
			switch(read_mode) {
				case outside_all:
					switch(cur_char) {
						case '<':
							read_mode = in_crocodile;
                            record_mode = false;
                            if( sentence_buffer_len ) {
                                *sentence_buf_ptr++ = '\0';
                                sentence_buf_ptr = sentence_buffer;
/*                                 printf("Found a string >%s<[%d]\n", sentence_buf, sentence_buf_len);*/
                                sentence_buffer_len=0;
    /*                             if (trans_level != in_quotes)*/
/*                                 return;*/
                                check_and_print_phrase ( sentence_buffer, line_number, file_name);
    /*                             trans_level=waiting_for_underscore;*/
                            }
							break;
                        case '\n': // newline
                        case '\t': // tab
                        case '\r': // line feed
                        case ' ':
                            break; // don't record it unless you've already seen a non whitespace
						case '{':
							read_mode = first_curly_bracket;
							break;
                        case '`':
                            printf("\nI found something beyond description on line %d of file %s. I can't go on like this. I quit.\n", line_number, 
                                   file_name);
                            return 1;
						default: // any normal char
							record_mode=true;
					}
					if ( record_mode && !in_block_trans) {
						*sentence_buf_ptr++ = cur_char;
						if(++sentence_buffer_len >= SENTENCE_BUF_SIZE) {
							*sentence_buf_ptr = '\0';
							printf("\n----\nsentence buf len exceeded %d in file %s on line %d quitting: %s",
                                   sentence_buffer_len, file_name, line_number, sentence_buffer );
							return 3;
						}
					}
                    break;
                case in_javascript:
					switch(cur_char) {
						case '<':
							read_mode = in_crocodile;
							break;
// 						case '{':
// 							read_mode = first_curly_bracket;
// 							break;
					}
					break;
                case in_css:
					switch(cur_char) {
						case '<':
							read_mode = in_crocodile;
							break;
// 						case '{':
// 							read_mode = first_curly_bracket;
// 							break;
					}
					break;
				case in_crocodile:
					switch(cur_char) {
						case '>':
							read_mode = outside_all;
                            if(strcmp(shift_buf, "<script") == 0)
                                script_coming=true;
                            if(strcmp((shift_buf+1), "<style") == 0)
                                css_coming=true;
                            if(script_coming) {
                                read_mode=in_javascript;
                                script_coming=false;
                            } else if(css_coming) {
                                read_mode=in_css;
                                css_coming=false;
                            }
							break;
                        case ' ':
                            if(strcmp(shift_buf, "<script") == 0)
                                script_coming=true;
                            else if(strcmp((shift_buf+1), "<style") == 0)
                                css_coming=true;
                            break;
                        case '=':
                            if(strcmp((shift_buf+1), " title") == 0)
                                read_mode = found_title;
//                                 printf("\n>>>SB %s\n", shift_buf);
					}
					break;
                case found_title:
                    switch(cur_char) {
                        case '"':
                            read_mode = title_double_quoted;
                            break;
                        case '\'':
                            read_mode = title_single_quoted;
                            break;
                        default:
                            printf("This should not happen found title not sg or db");
                            return 5;
                    }
                    break;
                case title_single_quoted:
                case title_double_quoted:
//                     printf("[IQ]");
                    switch(cur_char) {
                        case '\'':
//                             printf("[TISQ%d]",read_mode);
                            if(read_mode==title_single_quoted)
                                read_mode = in_crocodile;
                            break;
                        case '"':
//                             printf("[TIDQ%d]",read_mode);
                            if(read_mode==title_double_quoted) {
//                                 printf("[IC]");
                                read_mode = in_crocodile;
                            }
                            break;
                    }
                    break;        
				case first_curly_bracket:
					switch(cur_char) {
						case '%':
							read_mode = django_func;
							break;
						case '{':
							read_mode = django_print;
							break;
						default:
							read_mode = outside_all;
					}
					break;
				case django_func:
					switch(cur_char) {
						case '%':
							read_mode = end_django_func;
							break;
                        case ' ':
                            if (strcmp(shift_buf, "cktrans")==0)
                                in_block_trans = !in_block_trans;
					}
					break;
				case django_print:
					switch(cur_char) {
						case '}':
							read_mode = end_django_print;
							break;
					}
					break;
                case end_django_func:
                case end_django_print:
                    switch(cur_char) {
                        case '}':
                            read_mode = outside_all;
                        default:
                            if(read_mode==end_django_func)
                                read_mode=django_func;
                            if(read_mode==end_django_print)
                                read_mode=django_print;
                    }
                    break;
                default:
                    printf("TSNH 102 unexpected read mode %d\n", read_mode);
                    return 102;
			}
			append_to_shift_buf(cur_char);
		}
	} else if (current_file_type == js_file ) {
		int just_backslashed = 0;

		for(char_no=0;char_no<MAX_FILE_LENGTH;char_no++) {
//			printf("l");
			cur_char = getc(fp);
			if (cur_char == EOF) {
// 				printf("EOF\n");
				break;
			}
			if( PRINT_SYNTAX ) print_color_char(cur_char, read_mode);

            if (cur_char=='\n') {line_number++; if (PRINT_SYNTAX && PRINT_LINE_NUMBER) printf("%d|", line_number);}
			switch(read_mode) {
				case outside_all:
					switch(cur_char) {
						case '/':
							read_mode = first_slash;
							break;
						case '\'':
							read_mode = in_single_quotes;
							if (trans_level == seen_first_bracket) {
								trans_level=in_quotes;
//								printf("in quotes %d\n", trans_level);
							}
							break;
						case '"':
							read_mode = in_double_quotes;
							if (trans_level == seen_first_bracket) {
								trans_level=in_quotes;
//								printf("in quotes %d\n", trans_level);
							}
							break;
                        case '`':
                            read_mode = in_backtick;
//                             printf("\nI found something beyond description on line %d of file %s. I can't go on like this. I quit.\n", line_number, 
//                                    file_name);
//                             return 1;
                            break;
						case '_':
							trans_level = found_underscore;
//							printf("found underscore %d\n", trans_level);
							break;
						case '(':
							if (trans_level==seen_first_bracket) {
								trans_level=waiting_for_underscore;
							} else if (trans_level==found_underscore) {
//								printf("resetting trans level B %d\n", trans_level);
								trans_level = seen_first_bracket;
//								printf("found first bracket %d\n", trans_level);
							}
							break;
						case ')':
							trans_level = waiting_for_underscore;
//							printf("resetting trans level A %d\n", trans_level);
							break;
					}
					break;
				case first_slash:
					switch(cur_char) {
						case '/':
							read_mode = single_line_comment;
							break;
						case '*':
							read_mode = multiline_comment;
							break;
						case '\\':
							just_backslashed=1;
// 							break; // no break
                        case ' ': // division symbol or regex I don't care about
                        case '\n': // wierd division
                            if(*(shift_buf+5)!='(') {
                                read_mode = outside_all;
                                break;
                            } // else bleed over below
						default:
                            if (*(shift_buf+5)==' ' || (*(shift_buf+5)=='!' && *(shift_buf+4)==' ') ||
                                *(shift_buf+5)=='('
                            ) {
                                read_mode = in_regex;
                            } else {
                                read_mode = outside_all;
                            }
					}
					break;
				case in_regex:
					switch(cur_char) {
						case '\\':
							just_backslashed=1;
							break;
						case '/':
                            if (just_backslashed) just_backslashed=0;
							else read_mode = outside_all;
                        default:
                            just_backslashed=0;
					}
					break;
				case in_backtick:
					switch(cur_char) {
						case '`':
							read_mode = outside_all;
					}
					break;
				case single_line_comment:
					switch(cur_char) {
						case '\n':
							read_mode = outside_all;
							break;
					}
					break;
				case multiline_comment:
					switch(cur_char) {
						case '*':
							read_mode = second_comment_star;
							break;
					}
					break;
				case second_comment_star:
					switch(cur_char) {
						case '/':
							read_mode = outside_all;
							break;
						case '*':
							break; // retain read more second_comment_star
						default:
							read_mode = multiline_comment;
					}
					break;
				case in_single_quotes:
				case in_double_quotes:
					record_mode = 0;
					switch(cur_char) {
						case '\'':
						case '"':
							if (just_backslashed) {
								just_backslashed=0;
								record_mode=1;
							} else if ((cur_char=='"' and read_mode==in_double_quotes) or
									   (cur_char=='\'' and read_mode==in_single_quotes)) {
								read_mode = outside_all;
								*sentence_buf_ptr++ = '\0';
								sentence_buf_ptr = sentence_buffer;
								sentence_buffer_len=0;
								if (trans_level != in_quotes) {
									check_and_print_phrase ( sentence_buffer, line_number, file_name);
                                }
								trans_level=waiting_for_underscore;
							} else record_mode=1; // i.e. we are in single and found double, or vice versa
							break;
						case '\\':
                            if(just_backslashed) just_backslashed=0;
							else just_backslashed=1;
							break;
						default:
							record_mode=1;
					}
					if ( record_mode ) {
						just_backslashed=0;
						*sentence_buf_ptr++ = cur_char;
						if(++sentence_buffer_len >= SENTENCE_BUF_SIZE) {
							*sentence_buf_ptr = '\0';
							printf("\n----\nsentence buf len exceeded %d in file %s on line %d quitting: %s",
                                   sentence_buffer_len, file_name, line_number, sentence_buffer );
							return 23;
						}
					}
					break;
                default:
                    printf("TSNH 4 unknown reading mode %d\n", read_mode);
                    return 4;
			}
			append_to_shift_buf(cur_char);
		}
	}
	if(char_no+2 > MAX_FILE_LENGTH) {
        printf("\nend prematurely closed on %d / %d\n", char_no, MAX_FILE_LENGTH);
        return 1;
    }
    if(read_mode != outside_all && read_mode != single_line_comment) {
        printf("Read mode corrupted to %d. Breaking.\n", read_mode);
        return 2;
    }
	fclose(fp);
	return 0;
}

// checks a phrase with phrase_invalid and prints it
int check_and_print_phrase (char *sentence_buf, int line_no, char *file_name) {
    if ( phrase_invalid (sentence_buf)) {
        if (SHOW_REJECTED)
            printf("%s:%d: \033[0;31m%s\033[0m\n", file_name, line_no, sentence_buf);
        return 1;
    } else {
		printf("%s:%d >\033[0;32m", file_name, line_no);
        do {
            printf("%c", (*sentence_buf == '\n') ? '|' : *sentence_buf );
        } while(*( sentence_buf++) != '\0');
        printf("\033[0m<\n");
        number_of_trans_needed_found+=1;
        return 0;
	}
}

// uses a series of heuristics to guess whether a string is code, or English
int phrase_invalid (char *phrase ) {
	char * phrase_start = phrase;
//	strcmp() == 0
//	printf("> %s\n", sentence_buf);
	int has_dot = 0,
		has_caps = 0,
		has_non_caps=0,
		has_dash = 0,
		has_space=0,
		has_crocodile=0,
		has_equals=0,
		has_question_mark=0,
		has_underscore=0,
		has_amp=0,
		has_semi=0,
		has_hash=0,
		has_colon=0,
		has_squarebracket=0,
		has_curlybracket=0,
		has_leading_caps=0,
		has_percent=0,
		has_numbers=0,
		has_forwardslash=0,
		has_singlequote=0,
		has_comma=0,
        has_round_bracket=0,
        has_exclamation_mark=0,
        has_dollar=0,
		 has_doublequote=0;

	int buf_len=0;
	do {
		int buf_char = *phrase;
        // not a switch because we have complex cases
		if ( buf_char=='.') has_dot = 1;
		else if ( buf_char>=65 and buf_char<=90) {has_caps += 1;if(buf_len==0) has_leading_caps=1;}
		else if ( buf_char>=97 and buf_char<=122) has_non_caps += 1;
		else if ( buf_char>=48 and buf_char<=57) has_numbers += 1;
		else if ( buf_char==' ') has_space += 1;
		else if ( buf_char=='<' or buf_char=='>') has_crocodile += 1;
		else if ( buf_char=='=') has_equals += 1;
		else if ( buf_char=='-') has_dash += 1;
		else if ( buf_char=='?') has_question_mark += 1;
		else if ( buf_char=='!') has_exclamation_mark += 1;
		else if ( buf_char=='_') has_underscore += 1;
		else if ( buf_char=='/') has_forwardslash += 1;
		else if ( buf_char=='&') has_amp += 1;
		else if ( buf_char==';') has_semi += 1;
		else if ( buf_char=='#') has_hash += 1;
		else if ( buf_char=='$') has_dollar += 1;
		else if ( buf_char==':') has_colon += 1;
		else if ( buf_char==',') has_comma += 1;
		else if ( buf_char=='(' or buf_char==')') has_round_bracket += 1;
		else if ( buf_char=='{' or buf_char=='}') has_round_bracket += 1;
		else if ( buf_char=='%') has_percent += 1;
		else if ( buf_char=='\'') has_singlequote += 1;
		else if ( buf_char=='"') has_doublequote += 1;
		else if ( buf_char=='[' or buf_char==']') has_squarebracket += 1;
		buf_len++;
	} while (*phrase++ != '\0');
    
//     printf(" bl:%d ", buf_len);
    int has_symbol = has_dot+has_dash+has_crocodile+has_equals+has_underscore+has_amp+has_hash+has_semi+has_squarebracket+has_percent+has_doublequote+has_singlequote+has_comma+has_round_bracket+has_doublequote+has_exclamation_mark+has_dollar+has_curlybracket;

// 	if (has_crocodile<=1 and has_equals<=1 and (has_space or (!has_dot and !has_dash))) return 0;
	if(!has_caps and !has_non_caps) return 1;
	if(!has_caps and CERTAINTY>3) return 1;
    if(CERTAINTY>4) {
        if(has_crocodile > 1) return 1; // <label class="box-radio-label small" for="saturday">Sat</label>
        if(has_caps && !has_space && buf_len > 3) return 1; // MOUNTPOINT
        if(has_dash>1 and has_equals and has_doublequote) return 1; //     data-tippy-content="Explore directories
        if(has_equals==1 and (has_doublequote == 2 or has_singlequote==2) and !has_leading_caps) return 1;// title="Export Historic Data"
//         if(buf_len < 21 && (has_comma + has_underscore + has_dot + has_bracket) > 3) return 1; // ", ASSET_NAME.replace("
    }
    if(CERTAINTY>3) {
        if (buf_len<3) return 1; // console.log("A", clouds);
        if (buf_len<19 && has_space == 1 && has_caps > 1 && has_dot && has_non_caps > 7) return 1; // .slimScrollDiv ul
    }
	if(buf_len<38 and
			(has_dot+has_colon+has_dash+has_underscore+has_hash+has_singlequote+has_doublequote+has_equals) > 2 and
			!has_leading_caps and has_caps < 3)
		return 1; //  table.dataTable thead th:first-child
	if (buf_len<60 and (has_comma+has_dash)>4) return 1;
	if (buf_len<12 and ((has_forwardslash+has_colon+has_underscore) > 2)) return 1; // "Y/m/d H:i"
    if (buf_len>5 and has_symbol > (buf_len/10)) return 1; // >onclick="$('#mosaic-setup').modal('toggle');FORCE_SETUP = true;mosaic_setup()"><
	if(has_space) {
		if(has_caps) {
			if(!has_leading_caps and has_space==1 and buf_len < 14 and has_dot) return 1; //".slideThree "
			if(buf_len<5 and has_numbers ) return 1; // "0 B"
		} else {
			if(has_crocodile and has_equals and !has_dot) return 1;
			if(has_hash and has_underscore) return 1; // #user_properties_table tbody
			if(has_dot and has_dash and has_colon) return 1; // .ui-dialog-buttonpane button:first-child
			if(has_percent and has_squarebracket) return 1; // [%d] %c
		}
		if(has_dot and has_dash>2 and has_colon) return 1; // .ui-dialog-buttonpane button:first-child
	} else { // no space
		if(has_underscore and buf_len<10) return 1; // "AFP_Out"
		if(!has_non_caps and has_hash and buf_len < 9) return 1; // #5AC8FA
		if(has_caps) {
			if(has_non_caps and !has_leading_caps) return 1; // slideThree
			if(has_dot==1 and has_non_caps) return 1; // .slideThree
			if(!has_non_caps) {
                if( has_numbers>1 and has_colon and buf_len < 15) return 1; // T00:00:00
				if(has_underscore) return 1;
			}
		} else {
			if(has_hash==1) return 1; // #selector
			if(has_colon==1) return 1; // fishfish:
			if(has_forwardslash) return 1;
			if( has_numbers and has_non_caps) return 1;// 10px
			if(has_underscore and has_non_caps) return 1; // sdfsd_dfsdf
			if(has_squarebracket==2 and has_equals==1) return 1; //[name=user_selector]
			if(has_amp and has_semi) return 1;
		}
	}

	if (strcmp( phrase_start,"")==0) return 1;
	else if (strcmp( phrase_start,"\n")==0) return 1;
	else if (strcmp( phrase_start,"AZaz")==0) return 1;
	else if (strcmp( phrase_start,"POST")==0) return 1;
	else if (strcmp( phrase_start,"GET")==0) return 1;
	else if (strcmp( phrase_start,"DC")==0) return 1;
	else if (strcmp( phrase_start," °C")==0) return 1;
	else if (strcmp( phrase_start,"F j, Y")==0) return 1;
	else if (strcmp( phrase_start,"px Lato")==0) return 1;

    return 0; // if you got this far, then you're valid
}

// finds last dot in file path and checks extension
int get_file_type(char* file_name) {
	char *end_ptr = file_name;
	while (*++end_ptr != '\0'); // forward to end
	while (*end_ptr != '.' && *end_ptr != '/' && (&end_ptr != &file_name)) end_ptr--;
	if (strcmp(".js", end_ptr)==0) return js_file;
	else if (strcmp(".html", end_ptr)==0 or strcmp("mth.", end_ptr)==0) return html_file;
	else return unknown_file_type;
}

// instead of having a rotating buffer, I'm shifting it each time
void append_to_shift_buf(char letter) {
    char *shift_buf_ptr = shift_buf;
    do {
        if(*(shift_buf_ptr+1)=='\0') {
            *shift_buf_ptr = letter;
            break;
        } else {
            *(shift_buf_ptr) = *(shift_buf_ptr+1);
        }
    } while(*(shift_buf_ptr++)!='\0');
}

// turns this into a syntax highlighter - for debugging purposes
void print_color_char(char cur_char, int read_mode) {
	// 31 is red, 32 is yellow, 34 is cool blue
	int colour_code = 30;
    switch(read_mode) {
        case outside_all:
            colour_code=29; break;  // white
        case django_func:
        case django_print:
        case end_django_func:
        case end_django_print:
            colour_code=31; break; // red
        case in_single_quotes:
        case in_double_quotes:
        case title_double_quoted:
        case title_single_quoted:
            colour_code=34; break; // blue
        case in_crocodile:
            colour_code=33; break; // orange
        case in_javascript:
        case in_css:
        case first_slash:
        case single_line_comment:
        case second_comment_star:
        case multiline_comment:
            colour_code=35; break; // magenta
        case in_backtick:
        case in_regex:
            colour_code=36; break; // cyan
        default:
            printf("unknown read mode %d\n. dying.\n", read_mode);
            exit(0);
            colour_code=29;  // white
    }
	printf("\033[0;%dm", colour_code);
	printf("%c", cur_char);
	printf("\033[0m"); // end colour
}

// an ugly idea really - better to print the text as coloured
void print_read_mode(int read_mode) {
    switch(read_mode) {
        case django_func:
        case django_print:
            printf("[D]");
            break;
        case outside_all:
            printf("[O]");
//         default:
    }
}

// for debugging
void print_file_type(int file_type) {
    switch(file_type) {
        case html_file:
            printf("filetype html\n"); break;
        case js_file:
            printf("filetype js\n"); break;
        case unknown_file_type:
            printf("filetype other\n"); break;
        default:
            printf("This should not happen . unknow filetype");
    }
}
