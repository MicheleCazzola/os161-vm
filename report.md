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

## Architettura del progetto
Il progetto è stato realizzato utilizzando una _layer architecture_, con rare eccezioni, per cui ogni modulo fornisce astrazioni di un livello specifico, con dipendenze (quasi) unidirezionali. La descrizione dei moduli segue il loro livello di astrazione:
- ad alto livello, si interfacciano con moduli già esistenti, tra cui _runprogram.c_, _loadelf.c_, _main.c_, _trap.c_;
- ai livelli inferiori, forniscono servizi intermedi: segmento e page table sono dipendenze dell'address space (uno direttamente, l'altro indirettamente), coremap è dipendenza di diversi moduli (tra cui page table e quelli di livello superiore), analogamente a swapfile;
- altri sono moduli ausiliari: l'astrazione per il TLB è utilizzata in diversi moduli per accedere alle sue funzionalità, le statistiche sono calcolate solo invocando funzioni di interfaccia. 

## Codice sorgente
### Address space

### Gestore della memoria (pagevm)

### Segmento
Un processo è costituito da diversi segmenti, che sono aree di memoria aventi una semantica comune; nel nostro caso, ogni processo ha tre segmenti:
- codice: contiene il codice dell'eseguibile, è read-only; 
- dati: contiene le variabili globali, è read-write;
- stack: contiene gli stack frame delle funzioni chiamate durante l'esecuzione del processo, è read-write.

Essi non sono necessariamente contigui in memoria fisica, pertanto la soluzione adottata è la realizzazione di una page table per ognuno di essi; il numero di pagine necessarie è calcolato a valle della lettura dell'eseguibile, tranne che nel caso dello stack, in cui è costante.
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
- _seg_size_words_: memoria occupata dal segmento, considerando le parole intere;
- _elf_vnode_: puntatore al vnode del file eseguibile a cui il segmento appartiene;
- _page_table_: puntatore alla page table associata al segmento

Nel caso in cui la dimensione effettiva del segmento sia inferiore a quella occupata dal numero di pagine necessarie a salvarlo in memoria (ovvero in presenza di frammentazione interna), il residuo viene
completamente azzerato.

#### Implementazione
Le funzioni di questo modulo si occupano di svolgere operazioni a livello segmento, eventualmente agendo da semplici wrapper per funzioni di livello inferiore. Esse sono utilizzate all'interno dei moduli _addrspace_ o _pagevm_; i prototipi sono i seguenti:
```C
ps_t *seg_create(void);
int seg_define(
    ps_t *proc_seg, size_t seg_size_bytes, off_t file_offset, vaddr_t base_vaddr,
    size_t num_pages, size_t seg_size_words, struct vnode *elf_vnode, char read, char write, char execute
);
int seg_define_stack(ps_t *proc_seg, vaddr_t base_vaddr, size_t num_pages);
int seg_prepare(ps_t *proc_seg);
int seg_copy(ps_t *src, ps_t **dest);
paddr_t seg_get_paddr(ps_t *proc_seg, vaddr_t vaddr);
void seg_add_pt_entry(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
int seg_load_page(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
void seg_swap_out(ps_t *proc_seg, off_t swapfile_offset, vaddr_t vaddr);
void seg_swap_in(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
void seg_destroy(ps_t *proc_seg);
```

##### Creazione e distruzione
Si utilizzano le funzioni:
- _seg_create_: crea un nuovo segmento, allocando la memoria necessaria mediante _kmalloc_ e azzerandone tutti i campi, viene invocata alla creazione di un address space;
- _seg_destroy_: distrugge il segmento dato, liberando anche la memoria detenuta dalla propria page table, viene invocata alla distruzione dell'address space a cui il segmento appartiene.

##### Definizione, preparazione e copia
Si utilizzano le funzioni:
- _seg_define_: definisce il valore dei campi del segmento dato, utilizzando i parametri passati dal chiamante (ovvero _as_define_region_); essi non includono informazioni riguardanti la page table, che viene definita in seguito; tale funzione è utilizzata solo per le regioni di codice e dati, non per lo stack;
- _seg_define_stack_: come la precedente, ma utilizzata solo per lo stack e invocata da _as_define_stack_; per la natura dello stack:
    * esso non esiste all'interno del file, pertanto offset e dimensione nel file sono campi azzerati;
    * il numero di pagine è pari alla costante _PAGEVM_STACKPAGES_ che, coerentemente con quanto definito nativamente in os161, è pari a 18;
    * la dimensione occupata in memoria (parole intere) è legata direttamente al numero di pagine, secondo la costante _PAGE_SIZE_;
    * il puntatore al vnode è _NULL_, in quanto non è necessario mantenere tale informazione: essa è utilizzata, per le altre regioni, per caricare pagine in memoria dall'eseguibile, cosa che non avviene nel caso dello stack.
Essa effettua anche l'allocazione della page table, per coerenza con il pattern seguito dalla configurazione _DUMBVM_, in cui la funzione di preparazione non viene invocata sul segmento di stack.
- _seg_prepare_: utilizzata per allocare la page table relativa ai segmenti codice e dati, invocata una volta per ognuno dei segmenti, all'interno di _as_prepare_load_;
- _seg_copy_: effettua la copia in profondità di un segmento dato in un segmento destinazione, invocata in _as_copy_; si avvale dell'analoga funzione del modulo _pt_ per la copia della page table.

##### Operazioni di traduzione indirizzi
Si utilizzano le funzioni:
- _seg_get_paddr_: ottiene l'indirizzo fisico di una pagina di memoria, dato l'indirizzo virtuale che ha causato una TLB miss; è invocata da _vm_fault_, in seguito ad una TLB miss; utilizza direttamente la funzione analoga del modulo _pt_
- _seg_add_pt_entry_: aggiunge alla page table la coppia (indirizzo virtuale, indirizzo fisico), passati come parametri, utilizzando l'analoga funzione del modulo _pt_; viene invocata in _vm_fault_, in seguito ad una TLB miss.

##### Operazioni di swapping
Si utilizzano le funzioni:
- _seg_swap_out_: segna come _swapped out_ la pagina corrispondente all'indirizzo virtuale passato, mediante l'analoga funzione del modulo _pt_; è invocata da _getppage_user_ all'interno del modulo _coremap_, il quale effettua fisicamente lo swap out del frame;
- _seg_swap_in_: effettua le operazioni di swap in del frame corrispondente all'indirizzo virtuale fornito, supponendo vero che la pagina fosse in stato di _swapped_:
    * ottiene l'offset nello swapfile, a partire dal contenuto della page table alla entry opportuna;
    * effettua fisicamente l'operazione di swap in del frame, utilizzando l'indirizzo fisico fornito;
    * utilizza l'analoga funzione del modulo _pt_ per inserire la corrispondenza (indirizzo virtuale, indirizzo fisico) nella entry opportuna.
 
##### Loading (dinamico) di una pagina dall'eseguibile


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
