# ActualRamCleaner
Will actually clean your ram, but its limited because nowadays Os's already manage ram usage

# 💾 RAM Cleaner v2.1 — C++

Utilitário de linha de comando para monitorar e liberar memória RAM no **Linux** e **Windows**, com interface colorida no terminal.

---

## Requisitos

| Plataforma | Compilador | Dependências |
|------------|------------|--------------|
| Linux      | g++ 11+    | nenhuma (usa `/proc` e `sysinfo`) |
| Windows    | MSVC 2019+ / MinGW | `psapi.lib` (linkada automaticamente via `#pragma comment`) |

---

## Compilação

### Linux
```bash
g++ -std=c++17 -O2 -pthread -o ram_cleaner ram_cleaner.cpp
```

### Windows (MSVC — Developer Command Prompt)
```cmd
cl /std:c++17 /O2 /EHsc ram_cleaner.cpp /link psapi.lib
```

### Windows (MinGW / MSYS2)
```bash
g++ -std=c++17 -O2 -pthread -o ram_cleaner.exe ram_cleaner.cpp -lpsapi
```

---

## Execução

### Linux
```bash
# Sem root: mostra stats e faz limpeza limitada (sem drop_caches)
./ram_cleaner

# Com root: limpeza completa (recomendado)
sudo ./ram_cleaner
```

### Windows
```cmd
# Duplo clique ou via terminal
ram_cleaner.exe

# Com privilégios de administrador: limpeza completa (standby list)
# Clique direito → "Executar como administrador"
```

> **Nota:** sem permissões elevadas o programa ainda funciona, mas a limpeza fica limitada. O menu indica o status de admin na tela principal.

---

## Menu principal

```
╔══════════════════════════════════════════════════╗
║           💾 RAM CLEANER v2.1 — C++              ║
║                     Linux                         ║
╚══════════════════════════════════════════════════╝

  MEMÓRIA RAM
  Total     : 16.0 GB
  Em uso    : 9.2 GB
  Livre     : 6.8 GB
  Cache     : 1.4 GB

  Uso: [||||||||||||||......] 57.5%
  Swap: [.............]  0 MB / 4.0 GB

  Admin     : sim

  OPÇÕES
  [1] Limpar RAM agora
  [2] Monitorar ao vivo (atualiza a cada 2s)
  [3] Modo automático (limpa ao atingir threshold)
  [4] Atualizar stats
  [0] Sair
```

---

## Opções

### `[1]` Limpar RAM agora

Executa a limpeza imediatamente e exibe o que foi feito:

**Linux (com root):**
- `sync` — força escrita de buffers pendentes no disco
- `drop_caches 3` — libera pagecache, dentries e inodes
- `compact_memory` — desfragmenta a memória (se o kernel suportar)
- Ciclo `swapoff/swapon` — limpa o swap (só se houver RAM livre suficiente)

**Windows (com admin):**
- Esvazia o working set de todos os processos (`EmptyWorkingSet`)
- Purga a standby list via `NtSetSystemInformation`

**Windows (sem admin):**
- Apenas esvazia o working set de processos acessíveis

---

### `[2]` Monitorar ao vivo

Atualiza a tela a cada **2 segundos** com o uso atual de RAM e swap.

```
[Pressione Enter para voltar ao menu]
```

> Não precisa de Ctrl+C — basta pressionar Enter.

---

### `[3]` Modo automático

Monitora o uso de RAM em intervalos regulares e dispara a limpeza automaticamente quando o threshold é atingido.

Ao selecionar, dois parâmetros são pedidos:

| Parâmetro | Descrição | Padrão |
|-----------|-----------|--------|
| Threshold | % de uso de RAM para acionar a limpeza | `80` |
| Intervalo | Segundos entre cada verificação | `60` |

**Exemplo de saída:**
```
[14:32:01] check #1 — uso: 74%
[14:33:01] check #2 — uso: 83%
[14:33:01] threshold atingido! limpando...
[14:33:01]   sync: buffers escritos no disco
[14:33:01]   drop_caches: pagecache + dentries + inodes limpos
[14:33:01]   swap: sem swap ativo
[14:33:01] liberado: 1.2 GB | total sessão: 1.2 GB
```

> Pressione **Enter** para sair do modo automático.

---

### `[4]` Atualizar stats

Recarrega a tela com os dados de memória atuais sem executar nenhuma ação.

---

## Cores da barra de uso

| Cor | Faixa |
|-----|-------|
| 🟢 Verde | < 65% |
| 🟡 Amarelo | 65% – 84% |
| 🔴 Vermelho | ≥ 85% |

---

## Avisos importantes

- **Linux — ciclo swap:** o programa só executa `swapoff/swapon` se a RAM livre for maior que o swap em uso, evitando travamentos por falta de memória virtual.
- **Linux — drop_caches:** liberar cache é seguro; o kernel recarrega os dados do disco conforme necessário. Pode causar lentidão momentânea em sistemas com I/O intenso.
- **Windows — standby list:** requer privilégio `SeProfileSingleProcessPrivilege`, que é habilitado automaticamente se o processo for admin.
- O programa **não mata processos** nem altera configurações permanentes do sistema.

