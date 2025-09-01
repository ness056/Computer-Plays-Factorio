// Linux implementation of the OS dependent methods of FactorioInstance

#ifdef __linux__

#include "factorioAPI.hpp"

extern char **environ;

namespace ComputerPlaysFactorio {

    void FactorioInstance::InitStatic() {}
    
    bool FactorioInstance::Running() {
        int status;
        if (waitpid(m_process, &status, WNOHANG) == 0) return true;
        else {
            if (WIFEXITED(status)) {
                m_exitCode = WEXITSTATUS(status);
            } else {
                m_exitCode = EXIT_FAILURE;
            }
            return false;
        }
    }

    bool FactorioInstance::StartPrivate(const std::vector<const char*> &argv) {
        if (Running()) return false;
        if (!m_cleaned) Clean();
        m_cleaned = false;

        int stdinfd[2];
        int stdoutfd[2];

        if (pipe(stdinfd) != 0 || pipe(stdoutfd) != 0) {
            throw RuntimeErrorFormat("pipe failed: {}", strerror(errno()));
        }

        posix_spawn_file_actions_t actions;
        if (posix_spawn_file_actions_init(&actions)) {
            throw RuntimeErrorFormat("posix_spawn_file_actions_init failed: {}", strerror(errno()));
        }

        if (posix_spawn_file_actions_addclose(&actions, stdinfd[1]) != 0) {
            posix_spawn_file_actions_destroy(&actions);
            throw RuntimeErrorFormat("posix_spawn_file_actions_addclose failed: {}", strerror(errno()));
        }

        if (posix_spawn_file_actions_adddup2(&actions, stdinfd[0], STDIN_FILENO) != 0) {
            posix_spawn_file_actions_destroy(&actions);
            throw RuntimeErrorFormat("posix_spawn_file_actions_adddup2 failed: {}", strerror(errno()));
        }

        if (posix_spawn_file_actions_addclose(&actions, stdoutfd[0]) != 0) {
            posix_spawn_file_actions_destroy(&actions);
            throw RuntimeErrorFormat("posix_spawn_file_actions_addclose failed: {}", strerror(errno()));
        }

        if (posix_spawn_file_actions_adddup2(&actions, stdoutfd[1], STDOUT_FILENO) != 0) {
            posix_spawn_file_actions_destroy(&actions);
            throw RuntimeErrorFormat("posix_spawn_file_actions_adddup2 failed: {}", strerror(errno()));
        }

        if (posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO) != 0) {
            posix_spawn_file_actions_destroy(&actions);
            throw RuntimeErrorFormat("posix_spawn_file_actions_adddup2 failed: {}", strerror(errno()));
        }

        int err = posix_spawnp(
            &m_process,
            argv[0],
            &actions,
            nullptr,
            argv.data(),
            environ
        );

        m_inWrite = stdinfd[1];
        m_outRead = stdoutfd[0];
        close(stdinfd[0]);
        close(stdoutfd[1]);
        posix_spawn_file_actions_destroy(&actions);

        if (err != 0) {
            if (err == EPERM || err == ENOENT || err == ESRCH || err == EAGAIN || err == EACCES || err == EISDIR || err == EROFS) {
                Clean();
                return false;
            } else {
                throw RuntimeErrorFormat("posix_spawnp failed: {}", strerror(errno()));
            }
        }

        return true;
    }

    void FactorioInstance::Stop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!Running()) return;

        if (kill(m_process, 9)) {
            throw RuntimeErrorFormat("kill failed: {}", strerror(errno()));
        }

        CloseRCON();
    }

    void FactorioInstance::Clean() {
        if (m_cleaned) return;

        close(m_inWrite);
        close(m_outRead);

        m_cleaned = true;
    }

    bool FactorioInstance::Join() {
        if (m_process == 0) return false;

        int status;
        if (waitpid(m_process, &status, 0) != m_process) return false;

        if (WIFEXITED(status)) {
            m_exitCode = WEXITSTATUS(status);
        } else {
            m_exitCode = EXIT_FAILURE;
        }

        if (m_outListener.joinable()) m_outListener.join();

        return true;
    }
    
    int FactorioInstance::StdoutRead(char *buffer, int size) {
        int32_t readBytes;

        readBytes = read(m_outRead, buffer, size);
        if (readBytes == -1) {
            throw RuntimeErrorFormat("read failed: {}", strerror(errno()));
        }

        return readBytes;
    }
}

#endif