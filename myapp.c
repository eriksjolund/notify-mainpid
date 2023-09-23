#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <systemd/sd-daemon.h>

// TODO: check if fdname is unique
static int fd_from_fdname( char ** fdnames, int num_fdnames, char *fdname) {
  bool found = false;
  for (int i = 0; i < num_fdnames; i++) {
    if (strcmp(fdnames[i], fdname) == 0) {
      found = true;
      return i + SD_LISTEN_FDS_START;
    }
  }
  fprintf(stderr, "could not find fdname=%s\n", fdname);
  // TODO: rewrite this to not exit() inside the function
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr,"incorrect number of arguments\n");
    exit(EXIT_FAILURE);
  }
  char *resultfile_path = argv[1];

  char** fdnames = NULL;
  int num_fds = sd_listen_fds_with_names(0, &fdnames);
  if (num_fds != 1) {
    fprintf(stderr,"return value sd_listen_fds_with_names() != 2\n");
    exit(EXIT_FAILURE);
  }
  int mainpidfile_fd = fd_from_fdname(fdnames, num_fds, "mainpidfile");

  pid_t worker_pid;
  if((worker_pid = fork()) == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  }
  if (worker_pid == 0) {
    // child process from fork()
    //
    // /bin/sleep process will become the new MAINPID process
    execl("/bin/sleep", "/bin/sleep", "3600", NULL);
    exit(0);
  }

  int ret = write(mainpidfile_fd, &worker_pid, sizeof(worker_pid));
  if (ret != sizeof(worker_pid)) {
    fprintf(stderr, "failed to write to mainpidfile\n");
    exit(EXIT_FAILURE);
  }
  struct inotify_event *event;

  int inotify_fd = inotify_init();
  if (inotify_fd == -1) {
    perror("inotify_init");
    exit(EXIT_FAILURE);
  }

  int inotify_wd = inotify_add_watch(inotify_fd, resultfile_path, IN_ALL_EVENTS);
  if (inotify_wd == -1) {
    perror("inotify_add_watch");
    exit(EXIT_FAILURE);
  }
  sd_notify(0, "READY=1");

  bool modified_seen = false;
  bool close_write_seen = false;

  for (;;) {
    char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    ssize_t  num_read = read(inotify_fd, buf, sizeof(buf));

    if (num_read <= 0) {
      fprintf(stderr, "error: unexpected read() return value non-positive\n");
      exit(EXIT_FAILURE);
    }

    for (char *p = buf; p < buf + num_read; ) {
      event = (struct inotify_event *) p;
      if (event->mask & IN_MODIFY) {
	modified_seen = true;
      }
      if (event->mask & IN_CLOSE_WRITE) {
	close_write_seen = true;
      }
      p += sizeof(struct inotify_event) + event->len;
    }

    if (modified_seen && close_write_seen) {
      inotify_rm_watch(inotify_fd, inotify_wd );
      close(inotify_fd);
      char readchar;

      int  resultfile_fd = open(resultfile_path, O_RDONLY);
      if (resultfile_fd < 0) {
	fprintf(stderr, "error: could not open resultfile");
	exit(EXIT_FAILURE);
      }

      ssize_t num_read = read(resultfile_fd, &readchar, 1);
      if (num_read <= 0) {
	fprintf(stderr, "error: unexpected return value from read() of resultfile\n");
	exit(EXIT_FAILURE);
      }
      close(resultfile_fd);
      fprintf(stdout, "readchar=%c\n", readchar);
      return 0;
    }
  }
  // should not end up here
  return 0;
}
