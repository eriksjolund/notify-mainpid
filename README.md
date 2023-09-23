# notify-mainpid
command to notify `MAINPID` to systemd from `ExecStartPost=`

__Status:__ Currently work-in-progress. An experiment to see if a technical solution works.
This idea is currently half-baked.

This GitHub project was created to test an architecture idea for
https://github.com/containers/podman/issues/12778

### Problem description

If you run a systemd system service with `User=` and `type=notify`,
systemd will not allow

```
  std::string msg = std::format("MAINPID={}\n", mainpid);
  sd_pid_notify(systemd_exec_pid, 0, msg.c_str());
```
when `mainpid` is not in the expected cgroup.

__systemd__ only allows such a MAINPID to be sent from root.

For more details, see the [systemd Git commit message](https://github.com/systemd/systemd/commit/db256aab13d8a89d583ecd2bacf0aca87c66effc).

### Suggested solution

Sketch:

1. __systemd__ starts `ExecStart=myapp /run/myservice.resultfile`
2. __myapp__ has an inherited file descriptor that originates from
   ```
   OpenFile=/run/no_user_access/myapp.service.mainpid:mainpidfile:truncate
   ```
3. __myapp__ starts a child process to run the workload (in this example we use `sleep 3600`)
4. __myapp__ writes the PID of the child process to the file descriptor named _mainpidfile_
5. __myapp__ calls the function `sd_notify(0, "READY=1");`
6. __systemd__ starts `ExecStartPost=+notify-mainpid /run/no_user_access/myapp.service.mainpid /run/myapp.service.result`. The process will be running as root because the first character of the `ExecStartPost=` value is a `+`
7. __notify-mainpid__ reads the mainpid from the filepath that was passed as the first command-line argument.
8. __notify-mainpid__ notifies the mainpid to systemd
9. __notify-mainpid__ writes `ok` to the filepath that was passed as the second command-line argument.
10. __myapp__ is watching the file _/run/myapp.service.result_ with inotify and notices when it has been written to.

### Demo

build and install software

```
g++ -std=c++20 notify-mainpid.cpp -lsystemd -o notify-mainpid
gcc myapp.c -lsystemd -o myapp
sudo cp notify-mainpid /usr/bin/notify-mainpid
sudo chcon --reference /usr/bin/cp /usr/bin/notify-mainpid
sudo chown root:root /usr/bin/notify-mainpid
sudo chmod 755 /usr/bin/notify-mainpid
sudo cp myapp /usr/bin/myapp
sudo chcon --reference /usr/bin/cp /usr/bin/myapp
sudo chown root:root /usr/bin/myapp
sudo chmod 755 /usr/bin/myapp
```

```
sudo useradd test
sudo cp myapp.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo mkdir /run/no_user_access
sudo chmod 700 /run/no_user_access
```

enable debug logging in systemd
```
kill -signal=SIGRTMIN+23 1
```

maybe it's required to turn off SELinux (TODO: check this)
```
sudo setenforce 0
```
(remember to turn on SELinux after testing this)

start the service

```
sudo systemctl start myapp.service
```

### Result

The service is active but you will see a warning message:

_Supervising process 6329 which is not our child. We'll most likely not notice when it exits._

```
[root@linux ~]# journalctl -xeu myapp.service | grep -A3  "MAINPID"
Aug 21 18:20:56 linux systemd[1]: myapp.service: Got notification message from PID 6330 (MAINPID=6329)
Aug 21 18:20:56 linux systemd[1]: myapp.service: New main PID 6329 belongs to service, we are happy.
Aug 21 18:20:56 linux systemd[1]: myapp.service: Supervising process 6329 which is not our child. We'll most likely not notice when it exits.
Aug 21 18:20:56 linux systemd[1]: myapp.service: Child 6330 belongs to myapp.service.
```

I now realize that __myapp__  also starts up fine without `notify-mainpid`
because the workload (`sleep 3600`) is not in an out-of-tree cgroup. Currently __myapp__ is not a
functional demo because of this.
(I need to learn more about cgroups to be able to put the workload in an out-of-tree cgroup)

Maybe __notify-mainpid__ is still useful for podman/conmon? I haven't tried it out yet. The OpenFile= file descriptor would have to be handled by Podman for example.

The only test I did with Podman was with some code that is pretty similar to notify-mainpid:
https://github.com/containers/podman/issues/12778#issuecomment-1586255815
(but without `OpenFile=` and inotify)
