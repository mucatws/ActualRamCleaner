# ActualRamCleaner
Will actually clean your ram, but its limited because nowadays Os's already manage ram usage



- Exemplo de saída:
[14:32:01] check #1 — uso: 74%
[14:33:01] check #2 — uso: 83%
[14:33:01] threshold atingido! limpando...
[14:33:01]   sync: buffers escritos no disco
[14:33:01]   drop_caches: pagecache + dentries + inodes limpos
[14:33:01]   swap: sem swap ativo
[14:33:01] liberado: 1.2 GB | total sessão: 1.2 GB

Pressione Enter para sair do modo automático.


[4] Atualizar stats
Recarrega a tela com os dados de memória atuais sem executar nenhuma ação.

Cores da barra de uso
🟢 Verde< 65%🟡 Amarelo65% – 84%🔴 Vermelho≥ 85%

Avisos importantes

Linux — ciclo swap: o programa só executa swapoff/swapon se a RAM livre for maior que o swap em uso, evitando travamentos por falta de memória virtual.
Linux — drop_caches: liberar cache é seguro; o kernel recarrega os dados do disco conforme necessário. Pode causar lentidão momentânea em sistemas com I/O intenso.
Windows — standby list: requer privilégio SeProfileSingleProcessPrivilege, que é habilitado automaticamente se o processo for admin.
O programa não mata processos nem altera configurações permanentes do sistema.
