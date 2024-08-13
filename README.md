# Progetto os161

#### Composizione
- Cazzola Michele
- Fabio Leone
- Forte Filippo

#### Installazione
- Installazione vm iniziale
  1. Scaricare vm .ova di Cabodi da dropbox, versione 2022
  2. Lanciare vscode (si apre in automatico sul progetto os161)
  3. Verificare che funzioni tutto: Run Config, Make Depend, Build and Install, con configurazione DUMBVM
  4. Verificare che funzioni "Run task (no debug)": provare "p testbin/palin", dovrebbe lanciare "Unknown syscall 55"
  5. Verificare che funzioni "Run task" + "Start debugging": dovrebbe fare lo stesso, ma almeno si possono mettere i breakpoint
  6. Stoppare tutti i task in esecuzione
- Configurazione git
  1. Installare git: sudo apt install git, password "os161user", serve scrivere "Y" 1-2 volte
  2. Configurare git:
     git config --global user.name "..."
     git config --global user.email "..."
  3. Generare coppia chiave pubblica-privata: ssh-keygen, serve qualche "Invio" durante la creazione
  4. Andare su github sul proprio profilo: Settings->SSH and GPG keys->Add SSH key e copiare la chiave pubblica presente in /home/os161user/.ssh/id_rsa.pub
- Aggiornamento os161
  1. Entrare in "os161/os161-base-2.0.3" ed effettuare "git clone link kern-updated", dove "link" è il link ssh della repository del progetto: alla domanda che appare, rispondere "yes"
  2. Eliminare la cartella "kern", copiare il contenuto di "kern-updated" nella cartella corrente
  3. Effettuare i punti 3-4-5 dell'installazione iniziale, usando la configurazione "PAGING": questa volta, il test "p testbin/palin" dovrebbe funzionare, così come quelli sui thread (tt1, tt2, tt3)
  4. Eliminare la cartella "kern-updated"
  5. Effettuare di nuovo il punto 3

#### Page table
Per-process page table, con una PT "segmentata"

#### Files/directories
- kern/vm: 
  1. coremap.c: freeframe list
  2. pt.c: page table management
  3. segments.c: segment management
  4. vm_tlb.c: tlb management
  5. swapfile.c: swapping management
  6. vmstats.c: statistics calculator
  7. addrspace.c: address space management
- kern/syscall
  1. loadelf.c: changed to allow On-Demand Page Loading
- kern/include
  1. coremap.h
  2. pt.h
  3. segments.h
  4. vm_tlb.h
  5. swapfile.h
  6. vmstats.h
  7. addrspace.h
