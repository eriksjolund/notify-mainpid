#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

#include <systemd/sd-daemon.h>
#include <format>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr,"error: incorrect number of arguments\n");
    exit(EXIT_FAILURE);
  }
  const char *mainpidfile = argv[1];
  const char *resultfile = argv[2];

  const char *systemd_exec_pid_str = getenv("SYSTEMD_EXEC_PID");
  if (systemd_exec_pid_str == NULL) {
    fprintf(stderr,"error: environment variable SYSTEMD_EXEC_PID is not set\n");
    exit(EXIT_FAILURE);
  }
  // TODO: improve pid string parsing
  pid_t systemd_exec_pid = atoi(systemd_exec_pid_str);

  int mainpidfile_fd = open(mainpidfile, O_RDONLY);
  if (mainpidfile_fd < 0) {
    fprintf(stderr, "error: could not open mainpidfile");
    exit(EXIT_FAILURE);
  }
  pid_t mainpid;

  ssize_t num_read = read(mainpidfile_fd, &mainpid, sizeof(mainpid));
  if (num_read != sizeof(mainpid)) {
    fprintf(stderr, "error: unexpected result from read() of mainpidfile");
    exit(EXIT_FAILURE);
  }
  std::string msg = std::format("MAINPID={}\n", mainpid);
  sd_pid_notify(systemd_exec_pid, 0, msg.c_str());
  const char *okstr = "ok\n";
  int resultfile_fd = open(resultfile, O_WRONLY);
  if (resultfile_fd < 0) {
    fprintf(stderr, "error: could not open resultfile");
    exit(EXIT_FAILURE);
  }
  write(resultfile_fd, okstr, strlen(okstr));
  close(resultfile_fd);
  return 0;
}
