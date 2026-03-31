#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>   // TCP_NODELAY
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <stdatomic.h>
#include <pthread.h>
#include <math.h>

#include "../MoorControl/motor.h"
#include "logger.h"

#define MOTOR_TYPE      1
#define ALL_MOTORS      4
#define PORT            5050
#define SPEED_SEND_PORT 5051

// ── Buffer sizes ──────────────────────────────────────────────────────────────
// MAX must be larger than the longest single command you'll ever send.
// 256 is safe; 100 was dangerously small.
#define MAX             256
#define ACCUM_SIZE      1024   // accumulator for incomplete TCP reads

typedef enum { false, true } bool;

// ── Shared state (protected by state_mutex) ───────────────────────────────────
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

static double  speedAngel            = 0;
static double  globalSpeed           = 0;
static double  currentDegreesOfSize1 = 0;
static double  currentDegreesOfSize2 = 0;
static bool    isSan                 = 0;
static bool    torquedoff            = 1;

// ── Atomic flags ──────────────────────────────────────────────────────────────
static atomic_bool is_running = ATOMIC_VAR_INIT(0);

// ── Socket fds (used in signal handler) ──────────────────────────────────────
static int connfd = -1;
static int sockfd = -1;

// ─────────────────────────────────────────────────────────────────────────────
// Motor helpers
// ─────────────────────────────────────────────────────────────────────────────

static int connect_to_all_motors(void)
{
    for (int i = 0; i < ALL_MOTORS; i++) {
        if (connectMotor(i, MOTOR_TYPE) != 0)
            return 1;
    }
    pthread_mutex_lock(&state_mutex);
    torquedoff = 0;
    pthread_mutex_unlock(&state_mutex);
    return 0;
}

static int disconnect_all_motors(void)
{
    for (int i = 0; i < ALL_MOTORS; i++)
        disconnectMotor(i, MOTOR_TYPE);
    pthread_mutex_lock(&state_mutex);
    torquedoff = 1;
    pthread_mutex_unlock(&state_mutex);
    return 0;
}

static void emergency_stop(void)
{
    setGoalSpeed(0, 2, MOTOR_TYPE);
    setGoalSpeed(0, 3, MOTOR_TYPE);
    disconnect_all_motors();
    log_msg("Emergency Stop triggered");
}

// ─────────────────────────────────────────────────────────────────────────────
// Signal handlers
// ─────────────────────────────────────────────────────────────────────────────

static void handle_sigint(int sig)
{
    (void)sig;
    log_msg("Exiting (SIGINT)");
    disconnect_all_motors();
    if (connfd >= 0) close(connfd);
    if (sockfd >= 0) close(sockfd);
    closeMotorPort();
    log_close();
    exit(0);
}

static void handle_sigterm(int sig)
{
    (void)sig;
    emergency_stop();
    if (connfd >= 0) close(connfd);
    if (sockfd >= 0) close(sockfd);
    closeMotorPort();
    log_close();
    exit(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Speed helper
// ─────────────────────────────────────────────────────────────────────────────

static void change_speed(double speed, int motor_id)
{
    // Caller must hold state_mutex or be the only writer
    double k = speedAngel / 100.0;
    if (k >  1.0) k =  1.0;
    if (k < -1.0) k = -1.0;

    double left  = speed * (1.0 - k);
    double right = speed * (1.0 + k);

    setGoalSpeed( left,  motor_id,     MOTOR_TYPE);
    setGoalSpeed(-right, motor_id + 1, MOTOR_TYPE);
}

// ─────────────────────────────────────────────────────────────────────────────
// Command parser  (operates on ONE null-terminated line, no newline)
// ─────────────────────────────────────────────────────────────────────────────

static void parse_line(const char *line, int connfd_local, bool *isLocked)
{
    const char *ptr = line;
    uint8_t motor_id = 0;

    while (*ptr) {

        // ── ESTOP — highest priority, check first ────────────────────────────
        if (strncmp(ptr, "ESTOP", 5) == 0) {
            emergency_stop();
            write(connfd_local, "estop\n", 6);
            ptr += 5;
            continue;
        }

        // ── lock ─────────────────────────────────────────────────────────────
        if (strncmp(ptr, "lock:", 5) == 0) {
            ptr += 5;
            long val = strtol(ptr, (char **)&ptr, 10);
            if (val == 1) {
                for (int i = 0; i < ALL_MOTORS; i++)
                    setGoalSpeed(0, i, MOTOR_TYPE);
                *isLocked = true;
                log_msg("Locked");
            } else if (val == 0) {
                *isLocked = false;
                log_msg("Unlocked");
            }
            continue;
        }

        // ── motor id ─────────────────────────────────────────────────────────
        if (strncmp(ptr, "motor:", 6) == 0) {
            ptr += 6;
            motor_id = (uint8_t)strtol(ptr, (char **)&ptr, 10);
            continue;
        }

        // ── angle ────────────────────────────────────────────────────────────
        if (strncmp(ptr, "angle:", 6) == 0) {
            ptr += 6;
            double angle = strtod(ptr, (char **)&ptr);
            if (angle >  180.0) angle -= 360.0;
            else if (angle < -180.0) angle += 360.0;
            if (!(*isLocked))
                rotateMotor(angle, motor_id, MOTOR_TYPE);
            continue;
        }

        // ── velocity ─────────────────────────────────────────────────────────
        if (strncmp(ptr, "velocity:", 9) == 0) {
            ptr += 9;
            double velocity = strtod(ptr, (char **)&ptr);
            setProfileVelocity(velocity, motor_id, MOTOR_TYPE);
            continue;
        }

        // ── san (steering angle) ─────────────────────────────────────────────
        if (strncmp(ptr, "san:", 4) == 0) {
            ptr += 4;
            double san = strtod(ptr, (char **)&ptr);
            pthread_mutex_lock(&state_mutex);
            speedAngel = san;
            bool san_active = isSan;
            double gs = globalSpeed;
            pthread_mutex_unlock(&state_mutex);
            if (san_active)
                change_speed(gs, motor_id);
            continue;
        }

        // ── speed ─────────────────────────────────────────────────────────────
        if (strncmp(ptr, "speed:", 6) == 0) {
            ptr += 6;
            double speed = strtod(ptr, (char **)&ptr);
            pthread_mutex_lock(&state_mutex);
            isSan = 1;
            globalSpeed = speed;
            double sa = speedAngel;
            pthread_mutex_unlock(&state_mutex);
            if (!(*isLocked)) {
                // inline change_speed with local sa to avoid double-lock
                double k = sa / 100.0;
                if (k >  1.0) k =  1.0;
                if (k < -1.0) k = -1.0;
                setGoalSpeed( speed * (1.0 - k), motor_id,     MOTOR_TYPE);
                setGoalSpeed(-speed * (1.0 + k), motor_id + 1, MOTOR_TYPE);
            }
            continue;
        }

        // ── torque ───────────────────────────────────────────────────────────
        if (strncmp(ptr, "torque:", 7) == 0) {
            ptr += 7;
            long torque = strtol(ptr, (char **)&ptr, 10);
            log_msg("torque command: %ld", torque);
            if (torque == 1) {
                if (connect_to_all_motors() == 0) {
                    log_msg("Torqued on");
                    write(connfd_local, "on\n", 3);
                }
            } else if (torque == 0) {
                disconnect_all_motors();
                log_msg("Torqued off");
                write(connfd_local, "off\n", 4);
            } else {
                log_msg("Invalid torque value");
            }
            continue;
        }

        // ── prot ─────────────────────────────────────────────────────────────
        if (strncmp(ptr, "prot:", 5) == 0) {
            ptr += 5;
            pthread_mutex_lock(&state_mutex);
            isSan = 0;
            pthread_mutex_unlock(&state_mutex);
            double prot = strtod(ptr, (char **)&ptr);
            float local_speed = (float)(prot / 45.0) * 5.0f;
            setGoalSpeed(-local_speed, motor_id,     MOTOR_TYPE);
            setGoalSpeed(-local_speed, motor_id + 1, MOTOR_TYPE);
            continue;
        }

        // ── twodegree ────────────────────────────────────────────────────────
        if (strncmp(ptr, "twodegree:", 10) == 0) {
            ptr += 10;
            double td = strtod(ptr, (char **)&ptr);
            double angle1 = -177.78 + td * 287.41;
            double angle2 =  177.78 - td * 287.41;
            pthread_mutex_lock(&state_mutex);
            currentDegreesOfSize1 = angle1;
            currentDegreesOfSize2 = angle2;
            pthread_mutex_unlock(&state_mutex);
            log_msg("twodegree td:%.3f ang1:%.3f ang2:%.3f", td, angle1, angle2);
            rotateMotor(angle1, motor_id,     MOTOR_TYPE);
            rotateMotor(angle2, motor_id + 1, MOTOR_TYPE);
            continue;
        }

        // ── wbr ──────────────────────────────────────────────────────────────
        if (strncmp(ptr, "wbr:", 4) == 0) {
            ptr += 4;
            double td = strtod(ptr, (char **)&ptr);
            double k = fabs(td) / 70.0;
            pthread_mutex_lock(&state_mutex);
            double a1 = currentDegreesOfSize1;
            double a2 = currentDegreesOfSize2;
            pthread_mutex_unlock(&state_mutex);

            if (td > 0)
                a1 = a1 + (-177.78 - a1) * k;
            else if (td < 0)
                a2 = a2 + (-109.63 - a2) * k;

            log_msg("wbr td:%.3f ang1:%.3f ang2:%.3f", td, a1, a2);
            rotateMotor(a1, motor_id,     MOTOR_TYPE);
            rotateMotor(a2, motor_id + 1, MOTOR_TYPE);
            continue;
        }

        // ── no match — advance one char ───────────────────────────────────────
        ptr++;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Control thread
// ─────────────────────────────────────────────────────────────────────────────

void *control_thread(void *arg)
{
    (void)arg;

    // ── Read secret from environment ─────────────────────────────────────────
    const char *SECRET = getenv("MOTOR_SECRET");
    if (!SECRET) {
        log_msg("MOTOR_SECRET not set — aborting");
        log_close();
        exit(1);
    }

    // ── Create TCP socket ─────────────────────────────────────────────────────
    struct sockaddr_in servaddr, cliaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        perror("bind"); close(sockfd); exit(EXIT_FAILURE);
    }
    if (listen(sockfd, 5) != 0) {
        perror("listen"); close(sockfd); exit(EXIT_FAILURE);
    }

    log_msg("TCP control server listening on port %d", PORT);
    atomic_store(&is_running, 1);

    // ── Accept loop ───────────────────────────────────────────────────────────
    while (1) {
        socklen_t len = sizeof(cliaddr);
        connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
        if (connfd < 0) { perror("accept"); continue; }

        // ── Disable Nagle — critical for low latency ─────────────────────────
        int nodelay = 1;
        setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

        // ── Enable keepalive so dead connections are detected ─────────────────
        int keepalive = 1;
        setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

        log_msg("Client connected");

        // ── Authenticate ──────────────────────────────────────────────────────
        char auth_buf[MAX];
        int n = read(connfd, auth_buf, sizeof(auth_buf) - 1);
        if (n <= 0) { close(connfd); continue; }
        auth_buf[n] = '\0';
        auth_buf[strcspn(auth_buf, "\r\n")] = '\0';

        if (strcmp(auth_buf, SECRET) != 0) {
            log_msg("Wrong secret: [%s]", auth_buf);
            write(connfd, "unauthorized\n", 13);
            close(connfd);
            continue;
        }

        // ── Send initial motor status ─────────────────────────────────────────
        double m1v = getProfileVelocity(0, MOTOR_TYPE);
        double m2v = getProfileVelocity(1, MOTOR_TYPE);
        double m1p = getPosition(0, MOTOR_TYPE);
        double m2p = getPosition(1, MOTOR_TYPE);
        double m1l = getLimitLow(0, MOTOR_TYPE);
        double m1u = getLimitUp (0, MOTOR_TYPE);
        double m2l = getLimitLow(1, MOTOR_TYPE);
        double m2u = getLimitUp (1, MOTOR_TYPE);

        pthread_mutex_lock(&state_mutex);
        currentDegreesOfSize1 = m1p;
        currentDegreesOfSize2 = m2p;
        pthread_mutex_unlock(&state_mutex);

        char reply[512];
        snprintf(reply, sizeof(reply),
            "{\"status\":\"accepted\","
            "\"motor1velocity\":%f,\"motor2velocity\":%f,"
            "\"motor1position\":%f,\"motor2position\":%f,"
            "\"motor1limitlow\":%f,\"motor1limitup\":%f,"
            "\"motor2limitlow\":%f,\"motor2limitup\":%f}",
            m1v, m2v, m1p, m2p, m1l, m1u, m2l, m2u);
        write(connfd, reply, strlen(reply));

        log_msg("Client accepted, status sent");

        // ── Command loop with TCP framing accumulator ─────────────────────────
        // FIX: we accumulate raw bytes until we have a complete \n-terminated
        // line, then parse it.  This handles partial reads correctly.
        char  accum[ACCUM_SIZE];
        int   accum_len = 0;
        bool  isLocked  = false;

        while (1) {
            // Leave room for null terminator
            int space = (int)sizeof(accum) - accum_len - 1;
            if (space <= 0) {
                // Buffer full with no newline — malformed client, reset
                log_msg("Accumulator overflow — resetting");
                accum_len = 0;
                continue;
            }

            int nr = read(connfd, accum + accum_len, space);
            if (nr == 0) { log_msg("Client disconnected"); break; }
            if (nr  < 0) { perror("read"); break; }
            accum_len += nr;

            // Process every complete line (\n terminated)
            char *start = accum;
            char *nl;
            while ((nl = (char *)memchr(start, '\n', accum_len - (start - accum))) != NULL) {
                *nl = '\0';

                // Strip \r if present
                int line_len = (int)(nl - start);
                if (line_len > 0 && start[line_len - 1] == '\r')
                    start[line_len - 1] = '\0';

                if (strcmp(start, "EXIT") == 0) {
                    log_msg("EXIT received");
                    goto client_done;   // break out of both loops cleanly
                }

                log_msg("cmd: %s", start);
                parse_line(start, connfd, &isLocked);

                start = nl + 1;
            }

            // Move leftover (incomplete line) to front of buffer
            int remaining = (int)(accum_len - (start - accum));
            if (remaining > 0)
                memmove(accum, start, remaining);
            accum_len = remaining;
        }

        client_done:
        log_msg("Closing client connection");
        close(connfd);
        connfd = -1;
        disconnect_all_motors();
    }

    atomic_store(&is_running, 0);
    close(sockfd);
    sockfd = -1;
    disconnect_all_motors();
    return NULL;
}

// ─────────────────────────────────────────────────────────────────────────────
// Speed broadcast thread  (10 Hz push to a separate port)
// ─────────────────────────────────────────────────────────────────────────────

void *send_speed_thread(void *arg)
{
    (void)arg;

    int srv_fd;
    struct sockaddr_in servaddr, cliaddr;

    log_msg("Speed broadcast thread starting on port %d", SPEED_SEND_PORT);

    srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) { perror("speed socket"); return NULL; }

    int opt = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(SPEED_SEND_PORT);

    if (bind(srv_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
        perror("speed bind"); close(srv_fd); return NULL;
    }
    if (listen(srv_fd, 5) != 0) {
        perror("speed listen"); close(srv_fd); return NULL;
    }

    log_msg("Speed thread listening");

    while (1) {
        socklen_t len = sizeof(cliaddr);
        int cli_fd = accept(srv_fd, (struct sockaddr *)&cliaddr, &len);
        if (cli_fd < 0) {
            if (atomic_load(&is_running)) perror("speed accept");
            continue;
        }

        // TCP_NODELAY on speed socket too
        int nd = 1;
        setsockopt(cli_fd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof(nd));

        log_msg("Speed client connected");

        while (atomic_load(&is_running)) {
            double gs1 = getGoalSpeed(2, MOTOR_TYPE);
            double gs2 = getGoalSpeed(3, MOTOR_TYPE);

            char buf[256];
            snprintf(buf, sizeof(buf),
                     "{\"motor2\":%.3f,\"motor3\":%.3f,\"timestamp\":%ld}\n",
                     gs1, gs2, (long)time(NULL));

            if (write(cli_fd, buf, strlen(buf)) <= 0) {
                log_msg("Speed client disconnected");
                break;
            }
            usleep(100000);   // 10 Hz
        }

        close(cli_fd);
    }

    close(srv_fd);
    return NULL;
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(void)
{
    signal(SIGSEGV, handle_sigterm);
    signal(SIGINT,  handle_sigint);
    signal(SIGTERM, handle_sigterm);

    log_init("server.log");
    openMotorPort();
    log_msg("Motor port opened");

    pthread_t control_td;
    if (pthread_create(&control_td, NULL, control_thread, NULL) != 0) {
        perror("pthread_create control");
        log_close();
        return 1;
    }

    // Uncomment to enable speed broadcast:
    // pthread_t speed_td;
    // pthread_create(&speed_td, NULL, send_speed_thread, NULL);
    // pthread_join(speed_td, NULL);

    log_msg("Control thread created");
    pthread_join(control_td, NULL);

    disconnect_all_motors();
    closeMotorPort();
    log_close();
    return 0;
}