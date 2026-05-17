#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cinttypes>   // SCNu64 — fix para sscanf com uint64_t
#include <ctime>
#include <mutex>
 
#ifdef _WIN32
    #define PLATFORM_WINDOWS
    #include <windows.h>
    #include <psapi.h>
    #include <tlhelp32.h>
    #pragma comment(lib, "psapi.lib")
#else
    #define PLATFORM_LINUX
    #include <unistd.h>
    #include <sys/sysinfo.h>
    #include <sys/types.h>
    #include <fstream>
    #include <cstdlib>
    #include <cerrno>
    #include <cstring>
#endif
 
// ─── Cores ANSI ────────────────────────────────────────────────────────────
namespace Color {
    const std::string RESET   = "\033[0m";
    const std::string BOLD    = "\033[1m";
    const std::string DIM     = "\033[2m";
    const std::string RED     = "\033[91m";
    const std::string YELLOW  = "\033[93m";
    const std::string GREEN   = "\033[92m";
    const std::string BLUE    = "\033[94m";
    const std::string CYAN    = "\033[96m";
    const std::string MAGENTA = "\033[95m";
}
 
// ─── Estrutura de memória ───────────────────────────────────────────────────
struct MemInfo {
    uint64_t total_mb   = 0;
    uint64_t used_mb    = 0;
    uint64_t free_mb    = 0;
    uint64_t cached_mb  = 0;   // Linux apenas
    uint64_t swap_total = 0;
    uint64_t swap_used  = 0;
    double   pct        = 0.0;
};
 
// ─── Mutex para saída (usado nos modos monitor/auto) ───────────────────────
static std::mutex g_cout_mutex;
 
// ─── Utilitários ───────────────────────────────────────────────────────────
std::string fmt_mb(uint64_t mb) {
    std::ostringstream oss;
    if (mb >= 1024)
        oss << std::fixed << std::setprecision(1) << static_cast<double>(mb) / 1024.0 << " GB";
    else
        oss << mb << " MB";
    return oss.str();
}
 
std::string pct_color(double pct) {
    if (pct >= 85.0) return Color::RED;
    if (pct >= 65.0) return Color::YELLOW;
    return Color::GREEN;
}
 
std::string make_bar(double pct, int width = 30) {
    int filled = static_cast<int>((pct / 100.0) * width);
    filled = std::clamp(filled, 0, width);
    std::string bar = "[";
    bar += pct_color(pct);
    bar += std::string(filled, '|');
    bar += Color::DIM;
    bar += std::string(width - filled, '.');
    bar += Color::RESET;
    bar += "]";
    return bar;
}
 
// FIX: substitui system("clear/cls") por sequência ANSI — sem fork/processo extra
void clear_screen() {
    std::cout << "\033[2J\033[H" << std::flush;
}
 
bool is_admin() {
#ifdef PLATFORM_WINDOWS
    BOOL admin = FALSE;
    PSID admin_group = nullptr;
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt_authority, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &admin_group)) {
        CheckTokenMembership(nullptr, admin_group, &admin);
        FreeSid(admin_group);
    }
    return admin == TRUE;
#else
    return geteuid() == 0;
#endif
}
 
// FIX: localtime_r (Linux) / localtime_s (Windows) — thread-safe
std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf {};
 
#ifdef PLATFORM_WINDOWS
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
 
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << tm_buf.tm_hour << ":"
        << std::setw(2) << tm_buf.tm_min  << ":"
        << std::setw(2) << tm_buf.tm_sec;
    return oss.str();
}
 
void log_msg(const std::string& msg, const std::string& color = Color::RESET) {
    std::lock_guard<std::mutex> lock(g_cout_mutex);
    std::cout << Color::DIM << "[" << timestamp() << "] " << Color::RESET
              << color << msg << Color::RESET << "\n";
}
 
// ─── Leitura de memória ────────────────────────────────────────────────────
#ifdef PLATFORM_WINDOWS
MemInfo get_mem_info() {
    MemInfo info;
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
 
    info.total_mb = ms.ullTotalPhys / (1024ULL * 1024ULL);
    info.free_mb  = ms.ullAvailPhys / (1024ULL * 1024ULL);
    info.used_mb  = info.total_mb - info.free_mb;
    info.pct      = static_cast<double>(ms.dwMemoryLoad);
 
    PERFORMANCE_INFORMATION pi;
    pi.cb = sizeof(pi);
    if (GetPerformanceInfo(&pi, sizeof(pi))) {
        uint64_t page       = static_cast<uint64_t>(pi.PageSize);
        info.swap_total     = (static_cast<uint64_t>(pi.CommitLimit) * page) / (1024ULL * 1024ULL);
        info.swap_used      = (static_cast<uint64_t>(pi.CommitTotal) * page) / (1024ULL * 1024ULL);
    }
    return info;
}
#else
MemInfo get_mem_info() {
    MemInfo info;
    struct sysinfo si {};
    sysinfo(&si);
 
    uint64_t unit = static_cast<uint64_t>(si.mem_unit);
    info.total_mb = (static_cast<uint64_t>(si.totalram)  * unit) / (1024ULL * 1024ULL);
    info.free_mb  = (static_cast<uint64_t>(si.freeram)   * unit) / (1024ULL * 1024ULL);
    info.used_mb  = info.total_mb - info.free_mb;
    info.pct      = 100.0 * static_cast<double>(info.used_mb) / static_cast<double>(info.total_mb);
 
    // FIX: usa SCNu64 em vez de %lu — correto para uint64_t em qualquer arch
    std::ifstream mi("/proc/meminfo");
    std::string line;
    uint64_t cached = 0, buffers = 0, available = 0;
    while (std::getline(mi, line)) {
        uint64_t val = 0;
        if (sscanf(line.c_str(), "Cached: %" SCNu64,       &val) == 1) cached    = val / 1024ULL;
        if (sscanf(line.c_str(), "Buffers: %" SCNu64,      &val) == 1) buffers   = val / 1024ULL;
        if (sscanf(line.c_str(), "MemAvailable: %" SCNu64, &val) == 1) available = val / 1024ULL;
    }
    info.cached_mb = cached + buffers;
    if (available > 0) {
        info.free_mb = available;
        info.used_mb = info.total_mb - available;
        info.pct     = 100.0 * static_cast<double>(info.used_mb) / static_cast<double>(info.total_mb);
    }
 
    info.swap_total = (static_cast<uint64_t>(si.totalswap)                        * unit) / (1024ULL * 1024ULL);
    info.swap_used  = ((static_cast<uint64_t>(si.totalswap) -
                        static_cast<uint64_t>(si.freeswap)) * unit) / (1024ULL * 1024ULL);
    return info;
}
#endif
 
// ─── Limpeza Windows ───────────────────────────────────────────────────────
#ifdef PLATFORM_WINDOWS
void enable_privilege(const wchar_t* name) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return;
    LUID luid {};
    if (LookupPrivilegeValueW(nullptr, name, &luid)) {
        TOKEN_PRIVILEGES tp {};
        tp.PrivilegeCount           = 1;
        tp.Privileges[0].Luid       = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    }
    CloseHandle(token);
}
 
struct CleanResult {
    uint64_t freed_mb = 0;
    std::vector<std::string> details;
};
 
CleanResult clean_memory() {
    CleanResult result;
    auto before = get_mem_info();
 
    // FIX: usa PROCESS_SET_QUOTA | PROCESS_QUERY_INFORMATION em vez de PROCESS_ALL_ACCESS
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        result.details.push_back("snapshot de processos: falhou");
        return result;
    }
    PROCESSENTRY32 pe {};
    pe.dwSize = sizeof(pe);
    int count = 0;
    if (Process32First(snap, &pe)) {
        do {
            HANDLE proc = OpenProcess(PROCESS_SET_QUOTA | PROCESS_QUERY_INFORMATION,
                                      FALSE, pe.th32ProcessID);
            if (proc) {
                EmptyWorkingSet(proc);
                CloseHandle(proc);
                count++;
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    result.details.push_back("working set: " + std::to_string(count) + " processos limpos");
 
    if (is_admin()) {
        enable_privilege(L"SeProfileSingleProcessPrivilege");
        enable_privilege(L"SeLockMemoryPrivilege");
 
        typedef NTSTATUS(WINAPI* NtSetSysInfo_t)(UINT, PVOID, ULONG);
        auto NtSetSysInfo = reinterpret_cast<NtSetSysInfo_t>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtSetSystemInformation"));
 
        if (NtSetSysInfo) {
            UINT cmd = 4; // MemoryPurgeStandbyList
            NTSTATUS st = NtSetSysInfo(80, &cmd, sizeof(cmd));
            if (st == 0)
                result.details.push_back("standby list: limpa com sucesso");
            else
                result.details.push_back("standby list: falhou (NTSTATUS=0x"
                    + [&]{ std::ostringstream o; o << std::hex << st; return o.str(); }() + ")");
        }
    } else {
        result.details.push_back("standby list: ignorada (sem admin)");
    }
 
    auto after    = get_mem_info();
    result.freed_mb = (after.free_mb > before.free_mb)
                      ? after.free_mb - before.free_mb : 0;
    return result;
}
#endif
 
// ─── Limpeza Linux ─────────────────────────────────────────────────────────
#ifdef PLATFORM_LINUX
struct CleanResult {
    uint64_t freed_mb = 0;
    std::vector<std::string> details;
};
 
CleanResult clean_memory() {
    CleanResult result;
 
    if (!is_admin()) {
        result.details.push_back("sem root — rode com sudo para limpeza completa");
        return result;
    }
 
    auto before = get_mem_info();
 
    // sync
    sync();
    result.details.push_back("sync: buffers escritos no disco");
 
    // drop_caches — FIX: verifica retorno da escrita
    {
        std::ofstream dc("/proc/sys/vm/drop_caches");
        if (dc.is_open()) {
            dc << "3\n";
            if (dc.good())
                result.details.push_back("drop_caches: pagecache + dentries + inodes limpos");
            else
                result.details.push_back("drop_caches: abriu mas falhou ao escrever");
            dc.close();
        } else {
            result.details.push_back("drop_caches: falhou ao abrir — "
                + std::string(strerror(errno)));
        }
    }
 
    // compact_memory — FIX: verifica retorno
    {
        std::ofstream cm("/proc/sys/vm/compact_memory");
        if (cm.is_open()) {
            cm << "1\n";
            if (cm.good())
                result.details.push_back("compact_memory: memória compactada");
            else
                result.details.push_back("compact_memory: falhou ao escrever");
        }
        // compact_memory pode não existir em kernels antigos — silencioso se não abre
    }
 
    // FIX: swap off/on seguro — verifica se há memória livre suficiente antes
    {
        auto mem_check = get_mem_info();
        if (mem_check.swap_total > 0 && mem_check.swap_used > 0) {
            // Só faz o ciclo se houver RAM livre suficiente para absorver o swap
            if (mem_check.free_mb >= mem_check.swap_used) {
                int r1 = system("swapoff -a 2>/dev/null");
                int r2 = system("swapon -a  2>/dev/null");
                if (r1 == 0 && r2 == 0)
                    result.details.push_back("swap: ciclo off/on concluído");
                else
                    result.details.push_back("swap: ciclo falhou (verifique /etc/fstab)");
            } else {
                result.details.push_back("swap: ciclo ignorado — RAM livre ("
                    + fmt_mb(mem_check.free_mb) + ") insuficiente para absorver swap em uso ("
                    + fmt_mb(mem_check.swap_used) + ")");
            }
        } else {
            result.details.push_back("swap: sem swap ativo");
        }
    }
 
    auto after    = get_mem_info();
    result.freed_mb = (after.free_mb > before.free_mb)
                      ? after.free_mb - before.free_mb : 0;
    return result;
}
#endif
 
// ─── Exibição ──────────────────────────────────────────────────────────────
void print_header() {
    std::cout << Color::BOLD << Color::CYAN;
    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║           💾 RAM CLEANER v2.1 — C++              ║\n";
#ifdef PLATFORM_WINDOWS
    std::cout << "║                    Windows                        ║\n";
#else
    std::cout << "║                     Linux                         ║\n";
#endif
    std::cout << "╚══════════════════════════════════════════════════╝\n";
    std::cout << Color::RESET;
}
 
void print_mem_stats(const MemInfo& m) {
    std::cout << "\n";
    std::cout << Color::BOLD << "  MEMÓRIA RAM\n" << Color::RESET;
    std::cout << "  Total     : " << Color::CYAN   << fmt_mb(m.total_mb) << Color::RESET << "\n";
    std::cout << "  Em uso    : " << pct_color(m.pct) << fmt_mb(m.used_mb) << Color::RESET << "\n";
    std::cout << "  Livre     : " << Color::GREEN  << fmt_mb(m.free_mb)  << Color::RESET << "\n";
#ifdef PLATFORM_LINUX
    if (m.cached_mb > 0)
        std::cout << "  Cache     : " << Color::DIM   << fmt_mb(m.cached_mb) << Color::RESET << "\n";
#endif
    std::cout << "\n  Uso: " << make_bar(m.pct)
              << " " << pct_color(m.pct)
              << std::fixed << std::setprecision(1) << m.pct << "%"
              << Color::RESET << "\n";
 
    if (m.swap_total > 0) {
        double sw_pct = 100.0 * static_cast<double>(m.swap_used)
                               / static_cast<double>(m.swap_total);
        std::cout << "  Swap: " << make_bar(sw_pct, 30)
                  << " " << fmt_mb(m.swap_used) << " / " << fmt_mb(m.swap_total) << "\n";
    }
 
    std::cout << "\n  Admin     : "
              << (is_admin()
                  ? Color::GREEN + std::string("sim")
                  : Color::YELLOW + std::string("não (limpeza limitada)"))
              << Color::RESET << "\n";
}
 
void print_menu() {
    std::cout << "\n" << Color::BOLD << "  OPÇÕES\n" << Color::RESET;
    std::cout << "  [1] Limpar RAM agora\n";
    std::cout << "  [2] Monitorar ao vivo (atualiza a cada 2s)\n";
    std::cout << "  [3] Modo automático (limpa ao atingir threshold)\n";
    std::cout << "  [4] Atualizar stats\n";
    std::cout << "  [0] Sair\n";
    std::cout << "\n  Escolha: " << std::flush;
}
 
// ─── Modo monitor ──────────────────────────────────────────────────────────
// FIX: flag de parada para saída limpa via 'q' ao invés de só Ctrl+C
static volatile bool g_running = true;
 
void mode_monitor() {
    std::cout << Color::YELLOW
              << "\n  Modo monitor — pressione Enter para sair\n\n"
              << Color::RESET << std::flush;
 
    // Thread para detectar Enter
    std::thread input_thread([]() {
        std::string dummy;
        std::getline(std::cin, dummy);
        g_running = false;
    });
 
    while (g_running) {
        clear_screen();
        {
            std::lock_guard<std::mutex> lock(g_cout_mutex);
            print_header();
            auto m = get_mem_info();
            print_mem_stats(m);
            std::cout << Color::DIM << "\n  [Enter para voltar ao menu]\n" << Color::RESET << std::flush;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
 
    input_thread.join();
    g_running = true; // reseta para próxima chamada
}
 
// ─── Modo automático ───────────────────────────────────────────────────────
void mode_auto() {
    int threshold = 80, interval = 60;
 
    std::cout << "\n  Threshold de uso % para disparar limpeza [80]: " << std::flush;
    std::string inp;
    std::getline(std::cin, inp);
    if (!inp.empty()) {
        try { threshold = std::stoi(inp); }
        catch (...) { threshold = 80; }
    }
 
    std::cout << "  Intervalo de verificação em segundos [60]: " << std::flush;
    std::getline(std::cin, inp);
    if (!inp.empty()) {
        try { interval = std::stoi(inp); }
        catch (...) { interval = 60; }
    }
 
    threshold = std::clamp(threshold, 1, 99);
    interval  = std::max(interval, 5);
 
    std::cout << Color::GREEN
              << "\n  Auto ativado — threshold: " << threshold
              << "% | intervalo: " << interval << "s\n"
              << "  Pressione Enter para sair.\n\n"
              << Color::RESET << std::flush;
 
    uint64_t total_freed = 0;
    int      check_count = 0;
 
    std::thread input_thread([]() {
        std::string dummy;
        std::getline(std::cin, dummy);
        g_running = false;
    });
 
    while (g_running) {
        // Espera em fatias de 1s para poder reagir ao flag rapidamente
        for (int i = 0; i < interval && g_running; ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));
 
        if (!g_running) break;
 
        auto m = get_mem_info();
        check_count++;
        log_msg("check #" + std::to_string(check_count) +
                " — uso: " + std::to_string(static_cast<int>(m.pct)) + "%", Color::DIM);
 
        if (m.pct >= static_cast<double>(threshold)) {
            log_msg("threshold atingido! limpando...", Color::YELLOW);
            auto result = clean_memory();
            total_freed += result.freed_mb;
            for (const auto& d : result.details)
                log_msg("  " + d, Color::DIM);
            log_msg("liberado: " + fmt_mb(result.freed_mb) +
                    " | total sessão: " + fmt_mb(total_freed), Color::GREEN);
        }
    }
 
    input_thread.join();
    g_running = true;
}
 
// ─── Main ──────────────────────────────────────────────────────────────────
int main() {
#ifdef PLATFORM_WINDOWS
    // Habilita sequências ANSI no Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode  = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
 
    uint64_t session_freed = 0;
 
    while (true) {
        clear_screen();
        print_header();
        auto m = get_mem_info();
        print_mem_stats(m);
 
        if (session_freed > 0)
            std::cout << "\n  " << Color::GREEN << "Total liberado nesta sessão: "
                      << fmt_mb(session_freed) << Color::RESET << "\n";
 
        print_menu();
 
        std::string choice;
        std::getline(std::cin, choice);
 
        if (choice == "1") {
            std::cout << Color::BLUE << "\n  Limpando RAM...\n" << Color::RESET << std::flush;
            auto result = clean_memory();
            for (const auto& d : result.details)
                log_msg(d, Color::DIM);
            session_freed += result.freed_mb;
            log_msg("concluído — liberado: " + fmt_mb(result.freed_mb), Color::GREEN);
            std::cout << "\n  Pressione Enter para continuar..." << std::flush;
            std::getline(std::cin, choice);
 
        } else if (choice == "2") {
            mode_monitor();
 
        } else if (choice == "3") {
            mode_auto();
 
        } else if (choice == "4") {
            continue;
 
        } else if (choice == "0") {
            std::cout << Color::CYAN << "\n  Saindo. Total liberado: "
                      << fmt_mb(session_freed) << Color::RESET << "\n\n";
            break;
        }
    }
 
    return 0;
}
