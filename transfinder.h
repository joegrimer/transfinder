
/* linguistic changes */
#define true 1
#define false 0
#define and &&
#define or ||
#define print printf

/* proper constants */
#define MAX_FILE_LENGTH 186124
#define SENTENCE_BUF_SIZE 3650
#define SHOW_REJECTED 0
#define CERTAINTY 5
#define PRINT_SYNTAX 0
#define PRINT_READ_MODE false
#define MAX_PATH_LEN 128
#define MAX_FILES_TO_FIND 1024
#define MAX_FILE_EXTENSION_LENGTH 6
#define VERBOSE false
#define PRINT_LINE_NUMBER 1

/* headers */
int check_and_print_phrase (char *sentence_buf, int line_no, char * file_name);
int phrase_invalid (char *phrase );
int get_file_type(char* file_name);
int parse_file(char * file_name);
int recurse_file(char * file_name);
void append_to_shift_buf(char letter);
void print_read_mode(int read_mode);
void print_color_char(char cur_char, int read_mode);
void print_file_type(int file_type);

enum known_file_types {
	js_file,
	html_file,
	unknown_file_type
};

/* a subtype of the js_read_modes */
enum js_trans_func_levels {
	waiting_for_underscore,
	found_underscore,
	seen_first_bracket,
	in_quotes
};

/* for the parser - out current parsing state changes how we read */
enum parse_read_modes {
	in_single_quotes,
	in_double_quotes,
	outside_all,
	first_slash,
	single_line_comment,
	multiline_comment,
	second_comment_star,
	in_crocodile, /* for html read mode */
	in_regex,
	first_curly_bracket,
	django_func,
	django_print,
	end_django_func,
	end_django_print,
    found_title,
    title_single_quoted,
    title_double_quoted,
    in_javascript,
    in_css,
    in_backtick
};

