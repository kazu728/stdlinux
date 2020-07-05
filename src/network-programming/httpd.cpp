#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <cstdlib>
#include <memory.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>

const int LINE_BUF_SIZE = 4096;
const int BLOCK_BUF_SIZE = 1024;
const char SERVER_NAME[] = "LittleHTTP";
const char SERVER_VERSION[] = "1.0";
const int HTTP_MINOR_VERSION = 0;

typedef void (*sighandler_t)(int);
static void install_signal_handlers(void);
static void trap_signal(int sig, sighandler_t handler);
static void signal_exit(int sig);
static void service(FILE *in, FILE *out, const char *docroot);
static struct HTTPRequest *read_request(FILE *in);
static void read_request_line(struct HTTPRequest *req, FILE *in);
static struct HTTPHeaderField *read_header_field(FILE *in);
static void upcase(char *str);
static void free_request(struct HTTPRequest *req);
static long content_length(struct HTTPRequest *req);
static char *lookup_header_field_value(struct HTTPRequest *req, char *name);
static void respond_to(struct HTTPRequest *req, FILE *out, const char *docroot);
static void do_file_response(struct HTTPRequest *req, FILE *out, const char *docroot);
static void method_not_allowed(struct HTTPRequest *req, FILE *out);
static void not_implemented(struct HTTPRequest *req, FILE *out);
static void not_found(struct HTTPRequest *req, FILE *out);
static void output_common_header_fields(struct HTTPRequest *req, FILE *out, char *status);
static struct FileInfo *get_fileinfo(const char *docroot, char *path);
static char *build_fspath(const char *docroot, char *path);
static void free_fileinfo(struct FileInfo *info);
static char *guess_content_type(struct FileInfo *info);
static void *xmalloc(size_t sz);
static void log_exit(char *fmt, ...);

struct HTTPHeaderField
{
  char *name;
  char *value;
  struct HTTPHeaderField *next;
};

struct HTTPRequest
{
  int protocol_minor_version;
  char *method;
  char *path;
  struct HTTPHeaderField *header;
  char *body;
  long length;
};

struct FileInfo
{
  char *path;
  long size;
  int ok;
};

static void *xmalloc(size_t sz)
{
  void *p;

  p = malloc(sz);
  if (!p)
    log_exit("Failed to allocate memory");

  return p;
}

int main(int argc, char const *argv[])
{
  if (argc != 2)
  {
    fprintf(stderr, "Usage: %s <docroot>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  install_signal_handlers();
  service(stdin, stdout, argv[1]);

  exit(EXIT_SUCCESS);

  return 0;
}
static void install_signal_handlers(void)
{
  trap_signal(SIGPIPE, signal_exit);
}

static void trap_signal(int sig, sighandler_t handler)
{
  struct sigaction act;

  act.sa_handler = handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESTART;
  if (sigaction(sig, &act, NULL) < 0)
    log_exit("sigaction() failed: %s", strerror(errno));
}

static void signal_exit(int sig)
{
  log_exit("exit by signal %d", sig);
}

static void service(FILE *in, FILE *out, const char *docroot)
{
  struct HTTPRequest *req;

  req = (HTTPRequest *)read_request(in);
  respond_to(req, out, docroot);

  free_request(req);
}

static struct HTTPRequest *read_request(FILE *in)
{

  struct HTTPRequest *req;
  struct HTTPHeaderField *h;

  req = (HTTPRequest *)xmalloc(sizeof(struct HTTPRequest));
  read_request_line(req, in);

  req->header = NULL;
  while (h = read_header_field(in))
  {
    h->next = req->header;
    req->header = h;
  }

  req->length = content_length(req);

  const int MAX_REQUEST_BODY_LENGTH = (1024 * 1024);

  if (req->length != 0)
  {
    if (req->length > MAX_REQUEST_BODY_LENGTH)
      log_exit("request body is too long");

    req->body = (char *)xmalloc(req->length);

    if (fread(req->body, req->length, 1, in) < 1)
      log_exit("failed to read request body");
  }

  return req;
}

static void respond_to(struct HTTPRequest *req, FILE *out, const char *docroot)
{
  if (strcmp(req->method, "GET") == 0)
    do_file_response(req, out, docroot);
  else if (strcmp(req->method, "HEAD") == 0)
    do_file_response(req, out, docroot);
  else if (strcmp(req->method, "POST") == 0)
    method_not_allowed(req, out);
  else
    not_implemented(req, out);
}

static void read_request_line(struct HTTPRequest *req, FILE *in)
{
  char buf[LINE_BUF_SIZE];
  char *path, *p;

  if (!fgets(buf, LINE_BUF_SIZE, in))
    log_exit("no request line");

  p = strchr(buf, ' ');
  if (!p)
    log_exit("parse errro on request line 1: %s", buf);
  *p++ = '\0';

  req->method = (char *)xmalloc(p - buf);
  strcpy(req->method, buf);
  upcase(req->method);

  path = p;
  p = strchr(path, ' ');
  if (!p)
    log_exit("parse errro on request line 2: %s", buf);

  *p++ = '\0';
  req->path = (char *)xmalloc(p - path);

  if (strncasecmp(p, "HTTP/1.", strlen("HTTP/1.")) != 0)
    log_exit("parse errro on request line 3: %s", buf);

  p += strlen("HTTP/1.");
  req->protocol_minor_version = atoi(p);
}

static struct HTTPHeaderField *read_header_field(FILE *in)
{
  struct HTTPHeaderField *h;

  char buf[LINE_BUF_SIZE];
  char *p;

  if (!fgets(buf, LINE_BUF_SIZE, in))
    log_exit("failed to read request header", strerror(errno));
  if (buf[0] == '\n' || (strcmp(buf, "\r\n") == 0))
    return NULL;

  p = strchr(buf, ':');
  if (!p)
    log_exit("parse eror on request header field: %s", buf);
  *p++ = '\0';
  h = (HTTPHeaderField *)xmalloc(sizeof(struct HTTPHeaderField));
  h->name = (char *)xmalloc(p - buf);

  strcpy(h->name, buf);

  p += strspn(p, " \t");
  h->value = (char *)xmalloc(strlen(p) + 1);

  strcpy(h->value, p);

  return h;
}

static long content_length(struct HTTPRequest *req)
{
  char *val;
  long len;

  val = lookup_header_field_value(req, "Content-Lnegth");
  if (!val)
    return 0;

  len = atoi(val);
  if (len < 0)
    log_exit("Neagative Content-Length value");

  return len;
}

static void do_file_response(struct HTTPRequest *req, FILE *out, const char *docroot)
{
  struct FileInfo *info;

  info = get_fileinfo(docroot, req->path);
  if (!info->ok)
  {
    free_fileinfo(info);
    not_found(req, out);

    return;
  }
  output_common_header_fields(req, out, "200 OK");
  fprintf(out, "Content-Length: %ld\r\n", info->size);
  fprintf(out, "Content-Type: %s\r\n", guess_content_type(info));
  fprintf(out, "\r\n");
  if (strcmp(req->method, "HEAD") != 0)
  {
    int fd;
    char buf[BLOCK_BUF_SIZE];
    ssize_t n;

    fd = open(info->path, O_RDONLY);
    if (fd < 0)
      log_exit("failed to open %s %s", info->path, strerror(errno));
    for (;;)
    {
      n = read(fd, buf, BLOCK_BUF_SIZE);
      if (n < 0)
        log_exit("failed to read %s %s", info->path, strerror(errno));
      if (n == 0)
        break;
      if (fwrite(buf, n, 1, out) < n)
        log_exit("failed to wirite to socket: %s", strerror(errno));
    }
    close(fd);
  }
  fflush(out);
  free_fileinfo(info);
}

static char * guess_content_type(struct FileInfo *info)
{
  return "text/plain";
}

static void method_not_allowed(struct HTTPRequest *req, FILE *out)
{
  output_common_header_fields(req, out, "405 Method Not Allowed");
  fprintf(out, "Content-Type: text/html\r\n");
  fprintf(out, "\r\n");
  fprintf(out, "<html>\r\n");
  fprintf(out, "<header>\r\n");
  fprintf(out, "<title>405 Method Not Allowed</title>\r\n");
  fprintf(out, "<header>\r\n");
  fprintf(out, "<body>\r\n");
  fprintf(out, "<p>The request method %s is not allowed</p>\r\n", req->method);
  fprintf(out, "</body>\r\n");
  fprintf(out, "</html>\r\n");
  fflush(out);
}

static void not_implemented(struct HTTPRequest *req, FILE *out)
{
  output_common_header_fields(req, out, "501 Not Implemented");
  fprintf(out, "Content-Type: text/html\r\n");
  fprintf(out, "\r\n");
  fprintf(out, "<html>\r\n");
  fprintf(out, "<header>\r\n");
  fprintf(out, "<title>501 Not Implemented</title>\r\n");
  fprintf(out, "<header>\r\n");
  fprintf(out, "<body>\r\n");
  fprintf(out, "<p>The request method %s is not implemented</p>\r\n", req->method);
  fprintf(out, "</body>\r\n");
  fprintf(out, "</html>\r\n");
  fflush(out);
}

static void not_found(struct HTTPRequest *req, FILE *out)
{
  output_common_header_fields(req, out, "404 Not Found");
  fprintf(out, "Content-Type: text/html\r\n");
  fprintf(out, "\r\n");
  if (strcmp(req->method, "HEAD") != 0)
  {
    fprintf(out, "<html>\r\n");
    fprintf(out, "<header><title>Not Found</title><header>\r\n");
    fprintf(out, "<body><p>File not found</p></body>\r\n");
    fprintf(out, "</html>\r\n");
  }
  fflush(out);
}

#define TIME_BUF_SIZE 64

static void output_common_header_fields(struct HTTPRequest *req, FILE *out, char *status)
{
  time_t t;
  struct tm *tm;
  char buf[TIME_BUF_SIZE];

  t = time(NULL);
  tm = gmtime(&t);
  if (!tm)
    log_exit("gmtime() failed: %s", strerror(errno));
  strftime(buf, TIME_BUF_SIZE, "%a, %d %b %Y %H:%M:%S GMT", tm);
  fprintf(out, "HTTP/1.%d %s\r\n", HTTP_MINOR_VERSION, status);
  fprintf(out, "Date: %s\r\n", buf);
  fprintf(out, "Server: %s/%s\r\n", SERVER_NAME, SERVER_VERSION);
  fprintf(out, "Connection: close\r\n");
}

char *lookup_header_field_value(struct HTTPRequest *req, char *name)
{
  struct HTTPHeaderField *h;

  for (h = req->header; h; h->next)
  {
    if (strcasecmp(h->name, name) == 0)
      return h->value;
  }
}

static struct FileInfo *get_fileinfo(const char *docroot, char *urlpath)
{
  struct FileInfo *info;
  struct stat st;

  info = (struct FileInfo *)xmalloc(sizeof(struct FileInfo));
  info->path = build_fspath(docroot, urlpath);

  info->ok = 0;
  if (lstat(info->path, &st) < 0)
    return info;
  if (!S_ISREG(st.st_mode))
    return info;

  info->ok = 1;
  info->size = st.st_size;

  return info;
}

static void do_file_response(struct HTTPRequest *req, FILE *out, char *docroot)
{
}

static char *build_fspath(const char *docroot, char *urlpath)
{
  char *path;

  path = (char *)xmalloc(strlen(docroot) + 1 + strlen(urlpath) + 1);
  sprintf(path, "%s%s", docroot, path);
  return path;
}

static void free_fileinfo(struct FileInfo *info)
{
  free(info->path);
  free(info);
}

static void free_request(struct HTTPRequest *req)
{
  struct HTTPHeaderField *h, *head;

  head = req->header;

  while (head)
  {
    h = head;
    head = head->next;
    free(h->name);
    free(h->value);
    free(h);
    free(req->method);
    free(req->path);
    free(req->body);
    free(req);
  }
}

static void upcase(char *str)
{
  char *p;

  for (p = str; *p; p++)
  {
    *p = (char)toupper((int)*p);
  }
}

static void log_exit(char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);

  fputc('\n', stderr);

  va_end(ap);

  exit(EXIT_FAILURE);
}
