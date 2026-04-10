// ─────────────────────────────────────────────────────────────────────
// fliq::bash_validation — Implementation
// Translated from Rust crates/runtime/src/bash_validation.rs (1005 LOC)
// ─────────────────────────────────────────────────────────────────────

#include "fliq/bash_validation.hpp"
#include "fliq/permission_enforcer.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

namespace fliq {

// ── Constants ────────────────────────────────────────────────────────

static const std::unordered_set<std::string> WRITE_COMMANDS = {
    "cp","mv","rm","mkdir","rmdir","touch","chmod","chown","chgrp",
    "ln","install","tee","truncate","shred","mkfifo","mknod","dd",
};

static const std::unordered_set<std::string> STATE_MODIFYING_COMMANDS = {
    "apt","apt-get","yum","dnf","pacman","brew","pip","pip3",
    "npm","yarn","pnpm","bun","cargo","gem","go","rustup",
    "docker","systemctl","service","mount","umount","kill","pkill",
    "killall","reboot","shutdown","halt","poweroff","useradd",
    "userdel","usermod","groupadd","groupdel","crontab","at",
};

static const std::vector<std::string> WRITE_REDIRECTIONS = {">",">>",">&"};

static const std::unordered_set<std::string> GIT_RO_SUBCOMMANDS = {
    "status","log","diff","show","branch","tag","stash","remote",
    "fetch","ls-files","ls-tree","cat-file","rev-parse","describe",
    "shortlog","blame","bisect","reflog","config",
};

static const std::vector<std::pair<std::string,std::string>> DESTRUCTIVE_PATTERNS = {
    {"rm -rf /",   "Recursive forced deletion at root"},
    {"rm -rf ~",   "Recursive forced deletion of home directory"},
    {"rm -rf *",   "Recursive forced deletion of all files"},
    {"rm -rf .",   "Recursive forced deletion of current directory"},
    {"mkfs",       "Filesystem creation will destroy existing data"},
    {"dd if=",     "Direct disk write — can overwrite partitions"},
    {"> /dev/sd",  "Writing to raw disk device"},
    {"chmod -R 777","Recursively setting world-writable perms"},
    {"chmod -R 000","Recursively removing all permissions"},
    {":(){ :|:& };:","Fork bomb"},
};

static const std::unordered_set<std::string> ALWAYS_DESTRUCTIVE = {"shred","wipefs"};

static const std::unordered_set<std::string> SEMANTIC_READ_ONLY = {
    "ls","cat","head","tail","less","more","wc","sort","uniq","grep",
    "egrep","fgrep","find","which","whereis","whatis","man","info",
    "file","stat","du","df","free","uptime","uname","hostname","whoami",
    "id","groups","env","printenv","echo","printf","date","cal","bc",
    "expr","test","true","false","pwd","tree","diff","cmp","md5sum",
    "sha256sum","sha1sum","xxd","od","hexdump","strings","readlink",
    "realpath","basename","dirname","seq","yes","tput","column","jq",
    "yq","xargs","tr","cut","paste","awk","sed",
};

static const std::unordered_set<std::string> NETWORK_COMMANDS = {
    "curl","wget","ssh","scp","rsync","ftp","sftp","nc","ncat",
    "telnet","ping","traceroute","dig","nslookup","host","whois",
    "ifconfig","ip","netstat","ss","nmap",
};

static const std::unordered_set<std::string> PROCESS_COMMANDS = {
    "kill","pkill","killall","ps","top","htop","bg","fg","jobs",
    "nohup","disown","wait","nice","renice",
};

static const std::unordered_set<std::string> PACKAGE_COMMANDS = {
    "apt","apt-get","yum","dnf","pacman","brew","pip","pip3",
    "npm","yarn","pnpm","bun","cargo","gem","go","rustup",
    "snap","flatpak",
};

static const std::unordered_set<std::string> SYSTEM_ADMIN_COMMANDS = {
    "sudo","su","chroot","mount","umount","fdisk","parted","lsblk",
    "blkid","systemctl","service","journalctl","dmesg","modprobe",
    "insmod","rmmod","iptables","ufw","firewall-cmd","sysctl",
    "crontab","at","useradd","userdel","usermod","groupadd",
    "groupdel","passwd","visudo",
};

static const std::vector<std::string> SYSTEM_PATHS = {
    "/etc/","/usr/","/var/","/boot/","/sys/",
    "/proc/","/dev/","/sbin/","/lib/","/opt/",
};

// ── Helpers ──────────────────────────────────────────────────────────

static size_t find_end_of_value(const std::string& s) {
    if (s.empty()) return std::string::npos;
    if (s[0] == '"' || s[0] == '\'') {
        char q = s[0];
        size_t i = 1;
        while (i < s.size()) {
            if (s[i] == q && (i == 0 || s[i-1] != '\\')) {
                ++i;
                while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i]))) ++i;
                return i < s.size() ? i : std::string::npos;
            }
            ++i;
        }
        return std::string::npos;
    }
    for (size_t i = 0; i < s.size(); ++i)
        if (std::isspace(static_cast<unsigned char>(s[i]))) return i;
    return std::string::npos;
}

std::string extract_first_command(const std::string& command) {
    std::string remaining = command;
    // Skip leading env var assignments (KEY=val cmd ...)
    while (true) {
        size_t start = 0;
        while (start < remaining.size() && std::isspace(static_cast<unsigned char>(remaining[start]))) ++start;
        remaining = remaining.substr(start);
        auto eq = remaining.find('=');
        if (eq != std::string::npos) {
            bool valid = true;
            for (size_t i = 0; i < eq; ++i) {
                char c = remaining[i];
                if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) { valid = false; break; }
            }
            if (valid && eq > 0) {
                auto after = remaining.substr(eq + 1);
                auto sp = find_end_of_value(after);
                if (sp != std::string::npos) {
                    remaining = after.substr(sp);
                    continue;
                }
                return "";
            }
        }
        break;
    }
    auto sp = remaining.find(' ');
    return sp != std::string::npos ? remaining.substr(0, sp) : remaining;
}

static std::string extract_sudo_inner(const std::string& command) {
    auto pos = command.find("sudo");
    if (pos == std::string::npos) return "";
    auto rest = command.substr(pos + 4);
    // skip flags
    std::string word;
    size_t i = 0;
    while (i < rest.size() && std::isspace(static_cast<unsigned char>(rest[i]))) ++i;
    while (i < rest.size()) {
        if (rest[i] == '-') {
            while (i < rest.size() && !std::isspace(static_cast<unsigned char>(rest[i]))) ++i;
            while (i < rest.size() && std::isspace(static_cast<unsigned char>(rest[i]))) ++i;
        } else break;
    }
    return rest.substr(i);
}

static std::string extract_git_subcommand(const std::string& command) {
    std::vector<std::string> parts;
    std::istringstream ss(command);
    std::string tok;
    while (ss >> tok) parts.push_back(tok);
    for (size_t i = 1; i < parts.size(); ++i)
        if (!parts[i].empty() && parts[i][0] != '-') return parts[i];
    return "";
}

// ── Validation functions ─────────────────────────────────────────────

ValidationOutcome validate_read_only(const std::string& command, PermissionMode mode) {
    if (mode != PermissionMode::ReadOnly) return {ValidationResult::Allow, ""};

    auto first = extract_first_command(command);
    if (WRITE_COMMANDS.count(first))
        return {ValidationResult::Block, "Command '" + first + "' modifies the filesystem and is not allowed in read-only mode"};
    if (STATE_MODIFYING_COMMANDS.count(first))
        return {ValidationResult::Block, "Command '" + first + "' modifies system state and is not allowed in read-only mode"};

    if (first == "sudo") {
        auto inner = extract_sudo_inner(command);
        if (!inner.empty()) {
            auto r = validate_read_only(inner, mode);
            if (r.result != ValidationResult::Allow) return r;
        }
    }

    for (auto& redir : WRITE_REDIRECTIONS)
        if (command.find(redir) != std::string::npos)
            return {ValidationResult::Block, "Command contains write redirection '" + redir + "' which is not allowed in read-only mode"};

    if (first == "git") {
        auto sub = extract_git_subcommand(command);
        if (sub.empty()) return {ValidationResult::Allow, ""};
        if (GIT_RO_SUBCOMMANDS.count(sub)) return {ValidationResult::Allow, ""};
        return {ValidationResult::Block, "Git subcommand '" + sub + "' modifies repository state and is not allowed in read-only mode"};
    }

    return {ValidationResult::Allow, ""};
}

ValidationOutcome check_destructive(const std::string& command) {
    for (auto& [pattern, warning] : DESTRUCTIVE_PATTERNS)
        if (command.find(pattern) != std::string::npos)
            return {ValidationResult::Warn, "Destructive command detected: " + warning};

    auto first = extract_first_command(command);
    if (ALWAYS_DESTRUCTIVE.count(first))
        return {ValidationResult::Warn, "Command '" + first + "' is inherently destructive and may cause data loss"};

    if (command.find("rm ") != std::string::npos &&
        command.find("-r") != std::string::npos &&
        command.find("-f") != std::string::npos)
        return {ValidationResult::Warn, "Recursive forced deletion detected — verify the target path is correct"};

    return {ValidationResult::Allow, ""};
}

static bool command_targets_outside_workspace(const std::string& command) {
    auto first = extract_first_command(command);
    bool is_write = WRITE_COMMANDS.count(first) || STATE_MODIFYING_COMMANDS.count(first);
    if (!is_write) return false;
    for (auto& sp : SYSTEM_PATHS)
        if (command.find(sp) != std::string::npos) return true;
    return false;
}

ValidationOutcome validate_mode(const std::string& command, PermissionMode mode) {
    if (mode == PermissionMode::ReadOnly) return validate_read_only(command, mode);
    if (mode == PermissionMode::WorkspaceWrite) {
        if (command_targets_outside_workspace(command))
            return {ValidationResult::Warn, "Command appears to target files outside the workspace — requires elevated permission"};
    }
    return {ValidationResult::Allow, ""};
}

ValidationOutcome validate_sed(const std::string& command, PermissionMode mode) {
    auto first = extract_first_command(command);
    if (first != "sed") return {ValidationResult::Allow, ""};
    if (mode == PermissionMode::ReadOnly && command.find(" -i") != std::string::npos)
        return {ValidationResult::Block, "sed -i (in-place editing) is not allowed in read-only mode"};
    return {ValidationResult::Allow, ""};
}

ValidationOutcome validate_paths(const std::string& command, const std::string& workspace) {
    if (command.find("../") != std::string::npos) {
        if (command.find(workspace) == std::string::npos)
            return {ValidationResult::Warn, "Command contains directory traversal pattern '../' — verify the target path resolves within the workspace"};
    }
    if (command.find("~/") != std::string::npos || command.find("$HOME") != std::string::npos)
        return {ValidationResult::Warn, "Command references home directory — verify it stays within the workspace scope"};
    return {ValidationResult::Allow, ""};
}

ValidationOutcome validate_command(const std::string& command, PermissionMode mode, const std::string& workspace) {
    auto r = validate_mode(command, mode);
    if (r.result != ValidationResult::Allow) return r;
    r = validate_sed(command, mode);
    if (r.result != ValidationResult::Allow) return r;
    r = check_destructive(command);
    if (r.result != ValidationResult::Allow) return r;
    return validate_paths(command, workspace);
}

CommandIntent classify_command(const std::string& command) {
    auto first = extract_first_command(command);

    if (SEMANTIC_READ_ONLY.count(first)) {
        if (first == "sed" && command.find(" -i") != std::string::npos) return CommandIntent::Write;
        return CommandIntent::ReadOnly;
    }
    if (ALWAYS_DESTRUCTIVE.count(first) || first == "rm") return CommandIntent::Destructive;
    if (WRITE_COMMANDS.count(first)) return CommandIntent::Write;
    if (NETWORK_COMMANDS.count(first)) return CommandIntent::Network;
    if (PROCESS_COMMANDS.count(first)) return CommandIntent::ProcessManagement;
    if (PACKAGE_COMMANDS.count(first)) return CommandIntent::PackageManagement;
    if (SYSTEM_ADMIN_COMMANDS.count(first)) return CommandIntent::SystemAdmin;

    if (first == "git") {
        auto sub = extract_git_subcommand(command);
        if (GIT_RO_SUBCOMMANDS.count(sub)) return CommandIntent::ReadOnly;
        return CommandIntent::Write;
    }
    return CommandIntent::Unknown;
}

}  // namespace fliq
