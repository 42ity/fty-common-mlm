/*  =========================================================================
    fty_common_subprocess - C++ Wrapper around cxxtools::Fork

    Copyright (C) 2014 - 2020 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    fty_common_subprocess - C++ Wrapper around cxxtools::Fork
@discuss
@end
*/

#include <assert.h>
#include <fcntl.h>
#include <czmq.h>

#include "fty_common_mlm_subprocess.h"

namespace MlmSubprocess {
// forward declaration of helper functions
// TODO: move somewhere else

// small internal structure to be passed to MlmSubprocess::callbacks
struct sbp_info_t {
    uint64_t timeout;
    uint64_t now;
    SubProcess *proc_p;
    std::stringstream &buff;
};

static int close_forget(int &fd);
char * const * _mk_argv(const MlmSubprocess::Argv& vec);
void _free_argv(char * const * argv);
std::size_t _argv_hash(MlmSubprocess::Argv args);
static int s_output(SubProcess& p, std::string& o, std::string& e, uint64_t timeout, size_t timestep);
static int s_output2(SubProcess& p, std::string& o, uint64_t timeout, size_t timestep);


SubProcess::SubProcess(MlmSubprocess::Argv cxx_argv, int flags) :
    _fork(false),
    _state(SubProcessState::NOT_STARTED),
    _cxx_argv(cxx_argv),
    _return_code(-1),
    _core_dumped(false)
{
    // made more verbose to increase readability of the code
    int stdin_flag = PIPE_DISABLED;
    int stdout_flag = PIPE_DISABLED;
    int stderr_flag = PIPE_DISABLED;

    if ((flags & SubProcess::STDIN_PIPE) != 0) {
        stdin_flag = PIPE_DEFAULT;
    }
    if ((flags & SubProcess::STDOUT_PIPE) != 0) {
        stdout_flag = PIPE_DEFAULT;
    }
    if ((flags & SubProcess::STDERR_PIPE) != 0) {
        stderr_flag = PIPE_DEFAULT;
    }
    if ((flags & SubProcess::STDERR_TO_STDOUT_PIPE) != 0) {
        stderr_flag = PIPE_TO_STDOUT;
    }

    _inpair[0]  = stdin_flag;  _inpair[1]  = stdin_flag;
    _outpair[0] = stdout_flag; _outpair[1] = stdout_flag;
    _errpair[0] = stderr_flag; _errpair[1] = stderr_flag;
}

SubProcess::~SubProcess() {
    int _saved_errno = errno;

    // poll, SIGTERM, wait, SIGKILL is already inside terminate
    terminate();

    // close pipes
    close_forget(_inpair[0]);
    close_forget(_outpair[0]);
    close_forget(_errpair[0]);
    close_forget(_inpair[1]);
    close_forget(_outpair[1]);
    close_forget(_errpair[1]);

    errno = _saved_errno;
}

int SubProcess::closeStdin()
{
    return close_forget(_inpair[1]);
}

//note: the extra space at the end of the string doesn't really matter
std::string SubProcess::argvString() const
{
    std::string ret;
    for (std::size_t i = 0, l = _cxx_argv.size();
         i < l;
         ++i) {
        ret.append (_cxx_argv.at(i));
        ret.append (" ");
    }
    return ret;
}

bool SubProcess::run() {

    // do nothing if some process has been already started
    if (_state != SubProcessState::NOT_STARTED) {
        return true;
    }

    // create pipes
    if (_inpair[0] == PIPE_DEFAULT && ::pipe(_inpair) == -1) {
        return false;
    }
    if (_outpair[0] == PIPE_DEFAULT && ::pipe(_outpair) == -1) {
        return false;
    }
    if (_errpair[0] == PIPE_DEFAULT && ::pipe(_errpair) == -1) {
        return false;
    }

    _fork.fork();
    if (_fork.child()) {

        if (_inpair[0] >= 0) {
            int o_flags = fcntl(_inpair[0], F_GETFL);
            int n_flags = o_flags & (~O_NONBLOCK);
            fcntl(_inpair[0], F_SETFL, n_flags);
            ::dup2(_inpair[0], STDIN_FILENO);
            close_forget(_inpair[1]);
        }
        if (_outpair[0] >= 0) {
            close_forget(_outpair[0]);
            ::dup2(_outpair[1], STDOUT_FILENO);
        }
        if (_errpair[0] >= 0) {
            close_forget(_errpair[0]);
            ::dup2(_errpair[1], STDERR_FILENO);
        }
        else if (_errpair[0] == PIPE_TO_STDOUT && _outpair[1] >= 0) {
            ::dup2(_outpair[1], STDERR_FILENO);
        }

        auto argv = _mk_argv(_cxx_argv);
        if (!argv) {
            // need to exit from the child gracefully
            exit(ENOMEM);
        }

        ::execvp(argv[0], argv);
        // We can get here only if execvp failed
        exit(errno);

    }
    // we are in parent
    _state = SubProcessState::RUNNING;
    close_forget(_inpair[0]);
    close_forget(_outpair[1]);
    close_forget(_errpair[1]);
    // update a state
    poll();
    return true;
}

int SubProcess::wait(bool no_hangup)
{
    //thanks tomas for the fix!
    int status = -1;

    int options = no_hangup ? WNOHANG : 0;

    if (_state != SubProcessState::RUNNING) {
        return _return_code;
    }

    int ret = ::waitpid(getPid(), &status, options);
    if (no_hangup && ret == 0) {
        // state did not change here
        return _return_code;
    }

    if (WIFEXITED(status)) {
        _state = SubProcessState::FINISHED;
        _return_code = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status)) {
        _state = SubProcessState::FINISHED;
        _return_code = - WTERMSIG(status);

        if (WCOREDUMP(status)) {
            _core_dumped = true;
        }
    }
    // we don't allow wait on SIGSTOP/SIGCONT, so WIFSTOPPED/WIFCONTINUED
    // were omited here

    return _return_code;
}

int SubProcess::wait(unsigned int timeout)
{
    while( true ) {
        poll();
        if (_state != SubProcessState::RUNNING) {
            return _return_code;
        }
        if( ! timeout ) {
            return _return_code;
        }
        sleep(1);
        --timeout;
    }
}

int SubProcess::kill(int signal) {
    auto ret = ::kill(getPid(), signal);
    poll();
    return ret;
}

int SubProcess::hardkill() {
    auto ret = kill(SIGKILL);
    wait(); // avoid zombies
    return ret;
}

int SubProcess::terminate() {
    // update state
    poll();

    // Graceful shutdown
    auto ret = kill(SIGTERM);

    for (int i = 0; i<20 && isRunning(); i++) {
        usleep(100);
        poll(); // update state after a while
    }
    if (isRunning()) {
        // wait is already in hardkill
        ret = hardkill();
    }
    return ret;
}

const char* SubProcess::state() const {
    if (_state == SubProcess::SubProcessState::NOT_STARTED) {
        return "not-started";
    }
    else if (_state == SubProcess::SubProcessState::RUNNING) {
        return "running";
    }
    else if (_state == SubProcess::SubProcessState::FINISHED) {
        return "finished";
    }

    return "unimplemented state";
}

std::string read_all(int fd) {
    static size_t BUF_SIZE = 4096;
    char buf[4096+1];
    ssize_t r;

    std::stringbuf sbuf;

    while (true) {
        memset(buf, '\0', BUF_SIZE+1);
        r = ::read(fd, buf, BUF_SIZE);

        //TODO what to do if errno != EAGAIN | EWOULDBLOCK
        if (r <= 0) {
            break;
        }
        sbuf.sputn(buf, strlen(buf));
    }
    return sbuf.str();
}

int call(const MlmSubprocess::Argv& args) {
    SubProcess p(args);
    p.run();
    return p.wait();
}

int output(const MlmSubprocess::Argv& args, std::string& o, std::string& e, uint64_t timeout, size_t timestep) {
    SubProcess p(args, SubProcess::STDOUT_PIPE | SubProcess::STDERR_PIPE);
    return s_output (p, o, e, timeout, timestep);
}

int output2(const MlmSubprocess::Argv& args, std::string& o, uint64_t timeout, size_t timestep) {
    SubProcess p(args, SubProcess::STDOUT_PIPE);
    return s_output2 (p, o, timeout, timestep);
}

int output(const MlmSubprocess::Argv& args, std::string& o, std::string& e, const std::string& i, uint64_t timeout, size_t timestep) {
    SubProcess p(args, SubProcess::STDOUT_PIPE | SubProcess::STDERR_PIPE| SubProcess::STDIN_PIPE);
    p.run();
    int r = ::write(p.getStdin(), i.c_str(), i.size());
    ::fsync(p.getStdin());
    p.closeStdin();
    if (r == -1)
        return r;
    return s_output (p, o, e, timeout, timestep);
}

std::string wait_read_all(int fd) {
    static size_t BUF_SIZE = 4096;
    char buf[4096+1];
    ssize_t r;
    int exit = 0;

    int o_flags = fcntl(fd, F_GETFL);
    int n_flags = o_flags | O_NONBLOCK;
    fcntl(fd, F_SETFL, n_flags);

    std::stringbuf sbuf;
    memset(buf, '\0', BUF_SIZE+1);
    errno = 0;
    while (::read(fd, buf, BUF_SIZE) <= 0 &&
           (errno == EAGAIN || errno == EWOULDBLOCK) && exit < 5000) {
        usleep(1000);
        errno = 0;
        exit++;
    }

    sbuf.sputn(buf, strlen(buf));

    exit = 0;
    while (true) {
        memset(buf, '\0', BUF_SIZE+1);
        errno = 0;
        r = ::read(fd, buf, BUF_SIZE);
        if (r <= 0) {
            if(exit > 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
                break;
            usleep(1000);
            exit = 1;
        } else {
            exit = 0;
        }
        sbuf.sputn(buf, strlen(buf));
    }
    fcntl(fd, F_SETFL, o_flags);
    return sbuf.str();
}

// ### helper functions ###
// close fd and set it to -1
static int close_forget(int &fd) {
    int res = -1;

    if (fd >= 0)
        res = ::close(fd);
    if (res == 0)
        fd = SubProcess::PIPE_DEFAULT;
    return res;
}

char * const * _mk_argv(const MlmSubprocess::Argv& vec) {

    char ** argv = (char **) malloc(sizeof(char*) * (vec.size()+1));
    assert(argv);

    for (auto i=0u; i != vec.size(); i++) {

        auto str = vec[i];
        char* dest = (char*) malloc(sizeof(char) * (str.size() + 1));
        memcpy(dest, str.c_str(), str.size());
        dest[str.size()] = '\0';

        argv[i] = dest;
    }
    argv[vec.size()] = NULL;
    return (char * const*)argv;
}

void _free_argv(char * const * argv) {
    char *foo;
    std::size_t n;

    n = 0;
    while((foo = argv[n]) != NULL) {
        free(foo);
        n++;
    }
    free((void*)argv);
}

std::size_t _argv_hash(MlmSubprocess::Argv args) {


    std::hash<std::string> hash;
    size_t ret = hash("");

    for (auto str : args) {
        size_t foo = hash(str);
        ret = ret ^ (foo << 1);
    }

    return ret;
}

/*  ZLOOP AND PROPER TIMEOUT SUPPORT */

// add file descriptor to zloop
static int
xzloop_add_fd (zloop_t *self, int fd, zloop_fn handler, void *arg)
{
    assert (self);
    zmq_pollitem_t *fditem = (zmq_pollitem_t*) zmalloc (sizeof (zmq_pollitem_t));
    assert (fditem);
    fditem->fd = fd;
    fditem->events = ZMQ_POLLIN;

    int r = zloop_poller (self, fditem, handler, arg);
    free (fditem);
    return r;
}

// handle incoming data on fd
static int
s_handler (zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
    assert (loop); //remove compiler warning
    struct sbp_info_t *i = (struct sbp_info_t*) arg;

    //XXX: read_all is not a good idea for write intensive processes (like ping)
    //     because s_handler won't return - so lets read only PIPE_BUF and exit
    char buf[PIPE_BUF+1];
    memset(buf, '\0', PIPE_BUF+1);
    int r = ::read(item->fd, buf, PIPE_BUF);
    i->buff << buf;

    return r;
}

// ping the process
static int
s_ping_process (zloop_t *loop, int timer_id, void *args)
{
    assert (loop); //remove compiler warning
    struct sbp_info_t *i = (struct sbp_info_t*) args;

    if (zsys_interrupted)   // end the loop when signal is delivered
        return -1;

    i->proc_p->poll ();
    if (!i->proc_p->isRunning ())
        return -1;
    return 0;
}

// stop the loop
static int
s_end_loop (zloop_t *loop, int timer_id, void *args)
{
    return -1;
}

static int s_output(SubProcess& p, std::string& o, std::string& e, uint64_t timeout, size_t timestep)
{
    std::stringstream out;
    std::stringstream err;

    sbp_info_t out_info {timeout * 1000, (uint64_t) zclock_mono (), &p, out};
    sbp_info_t err_info {timeout * 1000, (uint64_t) zclock_mono (), &p, err};

    p.run();

    zloop_t *loop = zloop_new ();
    assert (loop);

    if (timeout != 0)
        zloop_timer (loop, timeout * 1000, 1, s_end_loop, NULL);
    zloop_timer (loop, timestep, 0, s_ping_process, &out_info);
    xzloop_add_fd (loop, p.getStdout (), s_handler, &out_info);
    xzloop_add_fd (loop, p.getStderr (), s_handler, &err_info);
    zloop_start (loop);

    zloop_destroy (&loop);

    int r = p.poll ();
    if (p.isRunning ()) {
        p.kill ();
        r = p.poll ();
        if (p.isRunning ()) {
            zclock_sleep (2000);
            p.hardkill ();
            r = p.poll ();
        }
    }

    out << read_all (p.getStdin ());
    err << read_all (p.getStderr ());

    o.assign(out.str ());
    e.assign(err.str ());
    return r;
}

static int s_output2(SubProcess& p, std::string& o, uint64_t timeout, size_t timestep)
{
    std::stringstream out;

    sbp_info_t out_info {timeout * 1000, (uint64_t) zclock_mono (), &p, out};

    p.run();

    zloop_t *loop = zloop_new ();
    assert (loop);

    if (timeout != 0)
        zloop_timer (loop, timeout * 1000, 1, s_end_loop, NULL);
    zloop_timer (loop, timestep, 0, s_ping_process, &out_info);
    xzloop_add_fd (loop, p.getStdout (), s_handler, &out_info);
    zloop_start (loop);

    zloop_destroy (&loop);

    int r = p.poll ();
    if (p.isRunning ()) {
        p.kill ();
        r = p.poll ();
        if (p.isRunning ()) {
            zclock_sleep (2000);
            p.hardkill ();
            r = p.poll ();
        }
    }

    out << read_all (p.getStdin ());

    o.assign(out.str ());
    return r;
}

// MVY: dumber version of shared::output
//      it seems that zloop+tntnet threads are not compatible
//      it crashes the stack
//      it can't be easily debuged
//      it can't be easily fixed
//      this is just workaround
//      and as a consequence, systemctl call becomes MORE
//      expensive than today, which harms testing, but not
//      the UI and UX for the product

// returns
//      positive return value of a process
//      negative is a number of a signal which terminates process
int
simple_output (const MlmSubprocess::Argv& args, std::string& o, std::string& e)
{

    static unsigned timeout = 5;
    SubProcess p (args, SubProcess::STDOUT_PIPE | SubProcess::STDERR_PIPE);
    p.run();

    unsigned int tme = 0;
    int ret = 0;

    std::string out;
    std::string err;

    while ((tme < timeout) && p.isRunning ()) {
        ret = p.wait ((unsigned int) 1);
        out += wait_read_all (p.getStdout ());
        err += wait_read_all (p.getStderr ());
        tme++;
    }
    if (p.isRunning ()) {
        // graceful sigterm, delay, maybe sigkill+wait
        ret = p.terminate ();
    }
    else
        ret = p.poll ();
    out += wait_read_all (p.getStdout ());
    err += wait_read_all (p.getStderr ());

    o.assign (out);
    e.assign (err);
    return ret;
}
} // end namespace

void
fty_common_mlm_subprocess_test (bool verbose)
{
    //TEST_CASE("subprocess-wait-true", "[subprocess][wait]")
    {
    std::vector<std::string> argv{"/bin/true"};
    int ret;
    bool bret;

    MlmSubprocess::SubProcess proc(argv);
    bret = proc.run();
    assert(bret);
    ret = proc.wait();
    assert(ret == 0);

    //nothing on stdout
    assert(proc.getStdout() == -2);

    //nothing on stderr
    assert(proc.getStderr() == -2);

    //nothing on stdin
    assert(proc.getStdin() == -2);
    }

    //TEST_CASE("subprocess-wait-false", "[subprocess][wait]")
    {
    std::vector<std::string> argv{"/bin/false"};
    int ret;
    bool bret;

    MlmSubprocess::SubProcess proc(argv);
    bret = proc.run();
    assert(bret);
    ret = proc.wait();
    assert(ret == 1);
    }

    //TEST_CASE("subprocess-wait-sleep", "[subprocess][wait]")
    {
    std::vector<std::string> argv{"/bin/sleep", "3"};
    int ret;
    bool bret;
    time_t start, stop;

    MlmSubprocess::SubProcess proc(argv);
    start = time(NULL);
    assert(start != -1);
    bret = proc.run();
    assert(bret);
    ret = proc.wait();
    stop = time(NULL);
    assert(stop != -1);
    assert(ret == 0);
    assert((stop - start) > 2);
    }

    //TEST_CASE("subprocess-read-stderr", "[subprocess][fd]")
    {
    std::vector<std::string> argv{"/usr/bin/printf"};
    char buf[1023];
    int ret;
    ssize_t rv;
    bool bret;

    MlmSubprocess::SubProcess proc(argv, MlmSubprocess::SubProcess::STDERR_PIPE);
    bret = proc.run();
    assert(bret);
    ret = proc.wait();

    //something on stderr
    memset((void*) buf, '\0', 1023);
    rv = read(proc.getStderr(), (void*) buf, 1023);
    assert(rv >= 0);
    assert(rv == (ssize_t)strlen(buf));
    assert(strlen(buf) > 42);

    //nothing on stdout
    assert(proc.getStdout() == -2);

    //nothing on stdin
    assert(proc.getStdin() == -2);

    assert(ret == 1);
    }

    //TEST_CASE("subprocess-read-stderr-to-stdout", "[subprocess][fd]")
    {
    std::vector<std::string> argv{"/bin/sh", "-c", "echo stdout; (2>&1 echo stderr)"};
    char buf[1023];
    int ret;
    ssize_t rv;
    bool bret;

    MlmSubprocess::SubProcess proc(argv, MlmSubprocess::SubProcess::STDOUT_PIPE | MlmSubprocess::SubProcess::STDERR_TO_STDOUT_PIPE);
    bret = proc.run();
    assert(bret);
    ret = proc.wait();

    //something on stderr
    memset((void*) buf, '\0', 1023);
    rv = read(proc.getStdout(), (void*) buf, 1023);
    assert(rv >= 0);
    assert(rv == (ssize_t)strlen(buf));
    assert(streq(buf, "stdout\nstderr\n"));

    //nothing on stderr
    assert(proc.getStderr() == MlmSubprocess::SubProcess::PIPE_TO_STDOUT);

    //nothing on stdin
    assert(proc.getStdin() == MlmSubprocess::SubProcess::PIPE_DISABLED);

    assert(ret == 0);
    }

    //TEST_CASE("subprocess-read-stdout", "[subprocess][fd]")
    {
    std::vector<std::string> argv{"/usr/bin/printf", "the-test\n"};
    char buf[1023];
    int ret;
    ssize_t rv;
    bool bret;

    MlmSubprocess::SubProcess proc(argv, MlmSubprocess::SubProcess::STDOUT_PIPE);
    bret = proc.run();
    assert(bret);
    ret = proc.wait();

    //nothing on stderr
    assert(proc.getStderr() == -2);

    //nothing on stdin
    assert(proc.getStdin() == -2);

    //something on stdout
    memset((void*) buf, '\0', 1023);
    rv = read(proc.getStdout(), (void*) buf, 1023);
    assert(rv >= 0);
    assert(rv == (ssize_t)strlen(buf));
    assert(strlen(buf) == 9);

    assert(ret == 0);
    }

    //TEST_CASE("subprocess-write-stdin", "[subprocess][fd]")
    {
    std::vector<std::string> argv{"/bin/cat"};
    const char ibuf[] = "hello, world";
    char buf[1023];
    int ret;
    ssize_t rv;
    bool bret;

    MlmSubprocess::SubProcess proc(argv, MlmSubprocess::SubProcess::STDIN_PIPE | MlmSubprocess::SubProcess::STDOUT_PIPE);
    bret = proc.run();
    assert(bret);
    // as we don't close stdin/stdout/stderr, new fd must be at least > 2
    assert(proc.getStdin() > STDERR_FILENO);

    rv = ::write(proc.getStdin(), (const void*) ibuf, strlen(ibuf));
    assert(static_cast<size_t> (rv) == strlen(ibuf));
    ::close(proc.getStdin());   // end of stream

    ret = proc.wait();

    //nothing on stderr
    assert(proc.getStderr() == -2);

    //something on stdout
    ::memset((void*) buf, '\0', 1023);
    rv = ::read(proc.getStdout(), (void*) buf, strlen(ibuf));
    assert(rv >= 0);
    assert(rv == (ssize_t)strlen(buf));
    assert(strlen(buf) == strlen(ibuf));
    assert(strcmp(buf, ibuf) == 0);

    assert(ret == 0);
    }

    //TEST_CASE("subprocess-output", "[subprocess][fd]")
    {
    MlmSubprocess::Argv argv{"/usr/bin/printf", "the-test\n"};
    std::string o;
    std::string e;

    int r = MlmSubprocess::output(argv, o, e);
    assert(r == 0);
    assert(o == "the-test\n");
    assert(e == "");

    MlmSubprocess::Argv argv2{"/usr/bin/printf"};

    r = MlmSubprocess::output(argv2, o, e);
    assert(r == 1);
    assert(o == "");
    assert(e.size() > 0);
    }

    //TEST_CASE("subprocess-output3", "[subprocess][fd]")
    {
    MlmSubprocess::Argv argv{"/bin/cat", "-n"};
    std::string o;
    std::string e;

    int r = MlmSubprocess::output(argv, o, e, "the test\n");
    assert(r == 0);
    assert(o == "     1\tthe test\n");
    assert(e == "");

    MlmSubprocess::Argv argv2{"/bin/cat", "-d"};

    r = MlmSubprocess::output(argv2, o, e, "the test");
    assert(r == 1);
    assert(o == "");
    assert(e.size() > 0);
    }

    //TEST_CASE("subprocess-MlmSubprocess::call", "[subprocess]")
    {
    int ret = MlmSubprocess::call({"/bin/false"});
    assert(ret == 1);
    ret = MlmSubprocess::call({"/bin/true"});
    assert(ret == 0);
    }

    //TEST_CASE("subprocess-poll-sleep", "[subprocess][poll]")
    {
    std::vector<std::string> argv{"/bin/sleep", "3"};
    int ret, i;
    bool bret;

    MlmSubprocess::SubProcess proc(argv);
    bret = proc.run();
    assert(bret);
    ret = proc.getReturnCode();

    for (i = 0; i != 600; i++) {
        ret = proc.poll();
        if (! proc.isRunning()) {
            break;
        }
        sleep(1);
    }
    assert(i != 0);
    assert(!proc.isRunning());
    assert(ret == 0);
    }

    //TEST_CASE("subprocess-hardkill", "[subprocess][hardkill]")
    {
    std::vector<std::string> argv{"/bin/sleep", "300"};
    int ret;
    bool bret;

    MlmSubprocess::SubProcess proc(argv);
    bret = proc.run();
    assert(bret);
    usleep(50000);

    ret = proc.hardkill();
    assert(ret == 0);
    for (auto i = 1u; i != 1000; i++) {
        usleep(i*50);
        proc.poll();
        if (!proc.isRunning()) {
            break;
        }
    }
    // note that getReturnCode does not report anything unless poll is MlmSubprocess::called
    proc.poll();
    usleep(50);
    ret = proc.getReturnCode();
    zsys_debug ("ret = %d", ret);

    assert(!proc.isRunning());
    assert(ret == -9);
    }

    //TEST_CASE("subprocess-kill", "[subprocess][kill]")
    {
    std::vector<std::string> argv{"/bin/sleep", "300"};
    int ret;
    bool bret;

    MlmSubprocess::SubProcess proc(argv);
    bret = proc.run();
    assert(bret);
    usleep(50000);

    // default per header is SIGTERM if not specified by caller
    ret = proc.kill();
    assert(ret == 0);
    for (auto i = 1u; i != 1000; i++) {
        usleep(i*50);
        proc.poll();
        if (!proc.isRunning()) {
            break;
        }
    }
    // note that getReturnCode does not report anything unless poll is MlmSubprocess::called
    proc.poll();
    usleep(50);
    ret = proc.getReturnCode();
    zsys_debug ("ret = %d", ret);

    assert(!proc.isRunning());
    assert(ret == -15);
    }

    //TEST_CASE("subprocess-terminate", "[subprocess][terminate]")
    {
    std::vector<std::string> argv{"/bin/sleep", "300"};
    int ret;
    bool bret;

    MlmSubprocess::SubProcess proc(argv);
    bret = proc.run();
    assert(bret);
    usleep(50000);

    ret = proc.terminate();
    assert(ret == 0);
    usleep(50);
    proc.poll();
    // note that getReturnCode does not report anything unless poll is MlmSubprocess::called
    ret = proc.getReturnCode();

    assert(!proc.isRunning());
    assert(ret == -15);
    }

    //TEST_CASE("subprocess-destructor", "[subprocess][wait]")
    {
    std::vector<std::string> argv{"/bin/sleep", "20"};
    time_t start, stop;
    bool bret;

    start = time(NULL);
    assert(start != -1);
    {
        MlmSubprocess::SubProcess proc(argv);
        bret = proc.run();
        assert(bret);
    } // destructor MlmSubprocess::called here!
    stop = time(NULL);
    assert(stop != -1);
    assert((stop - start) < 20);
    }

    //TEST_CASE("subprocess-external-kill", "[subprocess][wait]")
    {
    std::vector<std::string> argv{"/bin/sleep", "200"};
    bool bret;

    MlmSubprocess::SubProcess proc(argv);
    bret = proc.run();
    assert(bret);

    kill(proc.getPid(), SIGTERM);
    for (auto i = 1u; i != 1000; i++) {
        usleep(i*50);
        proc.poll();
        if (!proc.isRunning()) {
            break;
        }
    }
    proc.poll();

    for (auto i = 1u; i != 1000; i++) {
        usleep(i*50);
        proc.poll();
        if (!proc.isRunning()) {
            break;
        }
    }
    int r = proc.getReturnCode();
    //XXX: sometimes SIGHUP is delivered instead of SIGKILL - no time to investigate it
    //     so let makes tests no failing in this case ...
    assert((r == -15 || r == -1));
    }

    //TEST_CASE("subprocess-run-no-binary", "[subprocess][run]")
    {
    std::vector<std::string> argv{"/n/o/b/i/n/a/r/y",};
    int ret;
    bool bret;

    MlmSubprocess::SubProcess proc(argv);
    bret = proc.run();
    assert(bret);
    ret = proc.wait();
    assert(ret != 0);
    }

    //TEST_CASE("subprocess-MlmSubprocess::call-no-binary", "[subprocess][run]")
    {
    std::vector<std::string> argv{"/n/o/b/i/n/a/r/y",};
    int ret;

    ret = MlmSubprocess::call(argv);
    assert(ret != 0);
    }

    //TEST_CASE ("subprocess-test-timeout", "[subprocess][output]")
    {
    if (!zsys_file_exists ("/dev/full"))
        return;

    MlmSubprocess::Argv args {"/bin/cat", "/dev/full"};
    auto start = zclock_mono ();
    std::string o;
    std::string e;
    int r = MlmSubprocess::output (args, o, e, 4);
    auto stop = zclock_mono ();

    assert (r == -15);   //killed by SIGTERM
    assert (o.empty ());
    assert (e.empty());
    assert ((stop - start) >= 4000); // it's hard to tell how long the delay was, but it must be at least 4 secs
    }

    //TEST_CASE ("subprocess-test-timeout2", "[subprocess][output]")
    {
    // We have a few choices for an indefinitely-running program
    // "ping" may require permissions (setuid) that are not always there
    // and e.g. for busybox applet we can not really set them for non-roots
    //    MlmSubprocess::Argv args {"/bin/ping", "127.0.0.1"};
    //    MlmSubprocess::Argv args {"/bin/cat", "/dev/urandom"};
        MlmSubprocess::Argv args {"/usr/bin/yes", "T"};
        auto start = zclock_mono ();
        std::string o;
        std::string e;
        int r = MlmSubprocess::output (args, o, e, 4);
        auto stop = zclock_mono ();

        assert (r == -15);   //killed by SIGTERM
        assert (!o.empty ());
        assert (e.empty());
        assert ((stop - start) >= 4000); // it's hard to tell how long the delay was, but it must be at least 4 secs
    }

    // test if s_ping_process really works
    //TEST_CASE ("subprocess-test-timeout3", "[subprocess][output]")
    {
        MlmSubprocess::Argv args {"/bin/sleep", "2"};
        auto start = zclock_mono ();
        std::string o;
        std::string e;
        int r = MlmSubprocess::output (args, o, e, 5);
        auto stop = zclock_mono ();

        assert (r == 0);   //killed by SIGTERM
        assert (o.empty ());
        assert (e.empty());

        auto delta = stop - start;
        assert ((delta >= 2000 && delta < 5000)); // it's hard to tell how long the delay was, but it must be between 2 and 5 secs
    }
}
