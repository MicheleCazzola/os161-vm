# Progetto OS161: C1 - paging
## Introduzione
Il progetto svolto riguarda la realizzazione di un gestore della memoria virtuale, che sfrutti il demand paging e il loading dinamico; il suo nome è _pagevm_, come si nota da uno dei file presenti nel progetto.
I file aggiunti alla configurazione DUMBVM seguono le indicazioni fornite nella traccia del progetto, a cui si aggiunge il file pagevm.c (e relativo header file), contenente le funzioni di base per la gestione della memoria (dettagliate in seguito).
## Composizione e suddivisione carichi di lavoro

Il carico di lavoro è stato suddiviso in maniera abbastanza netta tra i componenti del gruppo, sfruttando Git:
- Filippo Forte: address space e gestore della memoria, modifica a file già esistenti nella configurazione DUMBVM;
- Michele Cazzola: segmenti, page table, statistiche e astrazione per il TLB, con wrapper per funzioni già esistenti;
- Leone Fabio: coremap e gestione dello swapfile.

La comunicazione è avvenuta principalmente con videochiamate a cadenza settimanale, di durata pari a 2-3 ore ciascuna, durante le quali si è discusso il lavoro fatto ed è stato pianificato quello da svolgere.

## Implementazione
### Address space

### Gestore della memoria (pagevm)

### Segmento
Un processo è costituito da diversi segmenti, che sono aree di memoria aventi una semantica comune; nel nostro caso, ogni processo ha tre segmenti:
- codice: contiene il codice dell'eseguibile, è read-only; 
- dati: contiene le variabili globali, è read-write;
- stack: contiene gli stack frame delle funzioni chiamate durante l'esecuzione del processo, è read-write.

Essi non sono necessariamente contigui in memoria fisica, pertanto una soluzione è data dalla realizzazione di una page table per ognuno di essi.
#### Strutture dati
La struttura dati che rappresenta il singolo segmento è definita in _segment.h_ ed è la seguente:
```C
typedef struct  {
    seg_permissions_t permissions;
    size_t seg_size_bytes;
    off_t file_offset;
    vaddr_t base_vaddr;
    size_t num_pages;
    size_t seg_size_words;
    struct vnode *elf_vnode;
    pt_t *page_table;
} ps_t;
```
I campi hanno il significato seguente:
- _permissions_: permessi associati al segmento, definiti dal tipo enumerativo:
```C
typedef enum {
    PAGE_RONLY,     /* 0: read-only */
    PAGE_RW,        /* 1: read-write */
    PAGE_EXE,       /* 2: executable */
    PAGE_STACK      /* 3: stack */
} seg_permissions_t;
```
- _seg_size_bytes_: dimensione del segmento nell'eseguibile, in bytes;
- _file_offset_: offset del segmento all'interno dell'eseguibile;
- _base_vaddr_: indirizzo virtuale iniziale del segmento;
- _num_pages_: numero di pagine occupate dal segmento, in memoria;
- _seg_size_words_: numero di parole di memoria occupate dal segmento;
- _elf_vnode_: puntatore al vnode del file eseguibile a cui il segmento appartiene;
- _page_table_: puntatore alla page table associata al segmento

Nel caso in cui la dimensione effettiva del segmento sia inferiore a quella occupata dal numero di pagine necessarie a salvarlo in memoria (ovvero in presenza di frammentazione interna), il residuo viene
completamente azzerato.

#### Implementazione

### Page table
Come detto in precedenza, sono presenti tre page table per ogni processo, una per ognuno dei tre segmenti che li costituiscono; per questa ragione, non sono necessarie forme di locking, in quanto la page table
non è una risorsa condivisa tra diversi processi, ma propria di un singolo processo.

#### Strutture dati
La struttura dati utilizzata per rappresentare la page table è definita in _pt.h_ ed è la seguente:
```C
typedef struct {
    unsigned long num_pages;
    vaddr_t base_vaddr;
    paddr_t *page_buffer;
} pt_t;
```
i cui campi hanno il significato seguente:
- _num_pages_: numero di pagine all'interno della page table;
- _base_vaddr_: indirizzo virtuale iniziale della page table, corrisponde con l'indirizzo virtuale di base del segmento e serve per calcolare la pagina di appartenenza di un indirizzo virtuale richiesto;
- _page_buffer_: vettore di indirizzi fisici delle pagine rappresentate, allocato dinamicamente in fase di creazione della page table.

Ogni entry della page table (ovvero ogni singolo elemento del buffer di pagine) può assumere i seguenti valori:
- PT_EMPTY_ENTRY (0): poiché 0 non è un indirizzo fisico valido (è occupato dal kernel), viene utilizzato per indicare una entry vuota, ovvero una pagina non ancora caricata in memoria;
- PT_SWAPPED_ENTRY (1): poiché 1 non è un indirizzo fisico valido (è occupato dal kernel), viene utilizzato per indicare una pagina di cui è stato effettuato swap out; dei 31 bit rimanenti, i meno significativi
  vengono utilizzati per rappresentare l'offset della pagina nello swapfile (esso ha dimensione 9 MB, pertanto sarebbero sufficienti 24 bit);
- altri valori: in questo caso è presente un indirizzo fisico valido per la pagina, ovvero essa è presente in memoria e non è avvenuto un page fault.

### TLB
Il modulo _vm_tlb.c_ (e relativo header file) contiene un'astrazione per la gestione e l'interfaccia con il TLB: non vengono aggiunte strutture dati, solo funzioni che svolgono funzione di wrapper (o poco più) rispetto alle funzioni di lettura/scrittura già esistenti, oltre alla gestione della politica di replacement.

#### Implementazione


### Statistiche
Il modulo _vmstats.c_ (e relativo header file) contiene le strutture dati e le funzioni per la gestione delle statistiche relative al gestore della memoria.

#### Strutture dati
Le strutture dati sono definite in _vmstats.c_, per poter implementare _information hiding_, accedendovi soltanto con funzioni esposte all'esterno. Esse sono:
```C
bool vmstats_active = false;
struct spinlock vmstats_lock;

unsigned int vmstats_counts[VMSTATS_NUM];

static const char *vmstats_names[VMSTATS_NUM] = {
    "TLB faults",                 /* TLB misses */
    "TLB faults with free",       /* TLB misses with no replacement */
    "TLB faults with replace",    /* TLB misses with replacement */
    "TLB invalidations",          /* TLB invalidations (number of times) */
    "TLB reloads",                /* TLB misses for pages stored in memory */
    "Page faults (zeroed)",       /* TLB misses requiring zero-filled page */
    "Page faults (disk)",         /* TLB misses requiring load from disk */
    "Page faults from ELF",       /* Page faults requiring load from ELF */
    "Page faults from swapfile",  /* Page faults requiring load from swapfile */
    "Swapfile writes"             /* Page faults requiring write on swapfile */
};
```
nell'ordine:
- _vmstats_active_: flag per indicare se il modulo è attivo (ovvero i contatori sono stati inizializzati opportunamente);
- _vmstats_lock_: spinlock per l'accesso in mutua esclusione, necessario in quanto tale modulo (con le sue strutture dati) è condiviso a tutti i processi e richiede che gli incrementi siano indipendenti;
- _vmstats_counts_: vettore di contatori, uno per ogni statistica;
- _vmstats_names_: vettore di stringhe, contenenti i nomi delle statistiche da collezionare, utile in fase di stampa

Nell'header file (_vmstats.h_) sono invece definiti:
```C
#define VMSTATS_NUM 10

enum vmstats_counters {
    VMSTAT_TLB_MISS,
    VMSTAT_TLB_MISS_FREE,
    VMSTAT_TLB_MISS_REPLACE,
    VMSTAT_TLB_INVALIDATION,
    VMSTAT_TLB_RELOAD,
    VMSTAT_PAGE_FAULT_ZERO,
    VMSTAT_PAGE_FAULT_DISK,
    VMSTAT_PAGE_FAULT_ELF,
    VMSTAT_PAGE_FAULT_SWAPFILE,
    VMSTAT_SWAPFILE_WRITE
};
```
ovvero:
- _VMSTATS_NUM_: il numero di statistiche da collezionare;
- _vmstats_counters_: i nomi delle statistiche da collezionare, utilizzati come indice in _vmstats_counts_ e _vmstats_names_, esposti all'esterno per poter essere utilizzati nell'invocazione della funzione di incremento

#### Implementazione
Le funzioni implementate in questo modulo hanno i prototipi seguenti:
```C
void vmstats_init(void);
void vmstats_increment(unsigned int stat_index);
void vmstats_show(void);
```
Esse non svolgono compiti particolarmente complessi:
- _vmstats_init_: inizializza il flag _vmstats_active_, dopo aver azzerato tutti i contatori in _vmstats_counts_; viene invocata al bootstrap del gestore della memoria virtuale;
- _vmstats_increment_: incrementa di un'unità la statistica associata all'indice fornito come parametro, effettuando il conteggio;
- _vmstats_show_: stampa, per ogni statistica, il valore di conteggio associato, mostrando eventuali messaggi di warning qualora le relazioni presenti tra le statistiche non fossero rispettate; viene invocata allo shutdown del gestore della memoria virtuale.

Ogni operazione effettuata all'interno delle funzioni di inizializzazione e incremento è protetta da spinlock, in quanto richiede accesso in mutua esclusione, poiché si realizzano scritture su dati condivisi; la funzione di stampa effettua solo letture, pertanto non richiede l'utilizzo di forme di locking.

### Coremap

### Swapfile
