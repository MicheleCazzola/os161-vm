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

Ogni processo possiede un address space che nel nostro caso viene rappresentato da 3 diversi segmenti: code, data e stack.

#### Struttura Dati

```C
typedef struct {
    ps_t *seg_code;
    ps_t *seg_data;
    ps_t *seg_stack;
} addrspace_t ;
```

#### Implementazione

Le funzioni di addrspace.c sono principalmente funzioni di alto livello che creano, gestiscono e distruggono l'address space appoggiandosi su funzioni di più basso livello implementate in segment.c. I prototipi corrispondenti sono:

```C
addrspace_t *as_create(void);
void as_destroy(addrspace_t *);
int as_copy(addrspace_t *src, addrspace_t **ret);
void as_activate(void);
void as_deactivate(void);
int as_prepare_load(addrspace_t *as);
int as_complete_load(addrspace_t *as);
int as_define_stack(addrspace_t *as, vaddr_t *initstackptr);
int as_define_region(addrspace_t *as,
                                   vaddr_t vaddr, size_t memsize,
                                   size_t file_size,
                                   off_t offset,
                                   struct vnode *v,
                                   int readable,
                                   int writeable,
                                   int executable);
ps_t *as_find_segment(addrspace_t *as, vaddr_t vaddr);
ps_t *as_find_segment_coarse(addrspace_t *as, vaddr_t vaddr);
```

##### Creazione e distruzione

Le funzioni _as_create_ e _as_destroy_ hanno il compito di allocare e liberare lo spazio di memoria necessario per ospitare la struttura dati; _as_destroy_ oltre a distruggere i 3 segmenti corrispondenti tramite la _seg_destroy_, ha anche il compito di chiudere il program file che viene lasciato aperto per permettere il loading dinamico. 

##### Copia e attivazione

Si utilizzano le funzioni:
- _as_copy_: si occupa di creare un nuovo address space e copiarci quello ricevuto come parametro. Si basa sulla _seg_copy_
- _as_activate_: questa funzione viene chiamata in _runprogram_ subito dopo aver creato e settato l'address space del processo. In particolare ha il compito di invalidare le entry della tlb e di inizializzare la vittima che eventualmente sarà sostituita nella tlb.

##### Define

Le funzioni _as_define_region_ e _as_define_stack_ vengono utilizzate per definire segmento codice e segmento dati per la _as_define_region_ e segmento stack per la _as_define_stack_. Esse fungono essenzialmente da wrapper per le relative funzioni di piu basso livelo. Ciò che aggiungo è il calcolo della dimensione dei relativi segmenti, dato necessario per le funzioni definite in segment.c.

##### Find

Sono presenti 2 funzioni che dato un address space e un indirizzo virtuale permettono di risalire al relativo segmento. Le due funzioni si differenziano nella granularità della ricerca, infatti la _as_find_segment_coarse_ controlla che l'indirizzo passato sia nei limiti dei vari segmenti ma allineati alla pagina. Entrambe le funzioni hanno il compito di calcolare inizio e fine dei 3 segmenti (code, data e stack) e verificare a quale di questi l'indirizzo passato appartiene.

### Gestore della memoria (pagevm)

In questo modulo vengono raccolte e utilizzate molte delle funzioni chiave implementate per questo progetto. Il modulo ha il compito di inizializzare e terminare l'intera gestione della memoria virtuale, oltre a gestire eventuali fault della memoria.

#### Implementazione

Le funzioni implementate in questo modulo sono:

```C
void vm_bootstrap(void);
void vm_shutdown(void);
int vm_fault(int faulttype, vaddr_t faultaddress);
void pagevm_can_sleep(void);
```

#### Inizializzazione e Terminazione

Le funzioni _vm_bootstrap_ e _vm_shutdown_ hanno il compito, rispettivamente, di inizializzare e distruggere tutte le strutture accessorie necessarie per la gestione della memoria virtuale. Tra queste strutture figurano la coremap, il sistema di swap, la TLB e il sistema di raccolta delle statistiche. Queste funzioni fungono essenzialmente da contenitori che richiamano le routine di inizializzazione e terminazione di altri moduli.

- **vm_bootstrap**: Questa funzione viene chiamata durante l'avvio del sistema per configurare l'intero sistema di memoria virtuale. Tra le operazioni principali, resetta il puntatore della vittima nella TLB, inizializza la coremap, imposta il sistema di swap e prepara il modulo di gestione delle statistiche.
  
- **vm_shutdown**: Questa funzione viene invocata durante lo spegnimento del sistema per rilasciare in modo sicuro le risorse utilizzate e stampare le statistiche sull'uso della memoria. Gestisce la chiusura del sistema di swap, della coremap e produce l'output delle statistiche raccolte.

#### Gestione dei Fault

La funzione centrale di questo modulo è _vm_fault_, che si occupa della gestione dei TLB miss.

- **vm_fault**: Questa funzione ha il compito di gestire la creazione di una nuova corrispondenza tra indirizzo fisico e indirizzo virtuale nella TLB ogni volta che si verifica un TLB miss. Il funzionamento della funzione si articola nei seguenti passaggi:

  1. **Verifica del Fault**: La funzione inizia verificando il tipo di fault (ad esempio, read-only, read, write) e assicurandosi che il processo corrente e il suo spazio di indirizzamento siano validi.
  
  2. **Recupero dell'Indirizzo Fisico**: Successivamente, recupera l'indirizzo fisico corrispondente all'indirizzo virtuale in cui si è verificato il fault utilizzando la funzione _seg_get_paddr_.
  
  3. **Gestione della Pagina**: Se il fault è dovuto a una pagina non ancora assegnata o a una pagina precedentemente swap-out, viene allocata una nuova pagina. In base al tipo di fault, vengono quindi chiamate le funzioni di basso livello _seg_add_pt_entry_ (per aggiungere la nuova pagina alla tabella delle pagine) o _seg_swap_in_ (per caricare la pagina dallo swap).
  
  4. **Aggiornamento della TLB**: Infine, utilizzando un algoritmo round-robin, viene scelta la vittima da sostituire nella TLB e la TLB viene aggiornata con la nuova corrispondenza indirizzo fisico-virtuale.

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

##### Loading (dinamico) di una pagina dall'eseguibile
Si utilizza la funzione _seg_load_page_, che costituisce buona parte della complessità di questo modulo e consente, di fatto, di implementare il loading dinamico delle pagine del programma dall'eseguibile. Dato il segmento associato, l'obiettivo è caricare in memoria la pagina associata ad un indirizzo virtuale (posto che essa non fosse né residente in memoria né _swapped_), ad un indirizzo fisico già opportunamente ricavato.

La pagina richiesta è rappresentata da un indice all'interno dell'eseguibile, calcolato a partire dal campo _base_vaddr_ del segmento; si possono presentare tre casi distinti:
- **prima pagina**: la pagina richiesta è la prima dell'eseguibile, il cui offset all'interno della pagina stessa potrebbe non essere nullo (ovvero, l'indirizzo virtuale di inizio dell'eseguibile potrebbe non essere _page aligned_) e, per semplicità, si mantiene tale offset anche a livello fisico, effettuando il caricamento della prima pagina anche se parzialmente occupata dall'eseguibile; ci sono due sottocasi possibili:
  * l'eseguibile termina nella pagina corrente
  * l'eseguibile occupa anche altre pagine
    
  tramite cui si determina quanti byte leggere dall'ELF file.
- **ultima pagina**: la pagina richiesta è l'ultima dell'eseguibile, di conseguenza il caricamento avviene ad un indirizzo _page aligned_, mentre l'offset all'interno dell'ELF file viene calcolato a partire dal numero di pagine totali e dall'offset all'interno della prima pagina; ci sono due sottocasi possibili:
  * l'eseguibile termina nella pagina corrente
  * l'eseguibile termina in una pagina precedente, ma la pagina corrente è ancora occupata: ciò è dovuto al fatto che un file eseguibile ha una _filesize_ e una _memsize_ che potrebbero differire, con la prima minore o uguale alla seconda; in tal caso, l'area di memoria occupata (ma non valorizzata) deve essere azzerata
  
  e, in particolare, nel secondo caso non si leggono byte dall'ELF file.
- **pagina intermedia**: il caricamento è analogo al caso precedente, a livello di offset nell'ELF file e di indirizzo fisico, ma si delineano tre sottocasi possibili, per quanto riguarda il numero di byte da leggere:
  * l'eseguibile termina in una pagina precedente
  * l'eseguibile termina nella pagina corrente
  * l'eseguibile occupa anche pagine successive
  
  i quali sono gestiti analogamente ai due casi precedenti.

Dopo aver definito i tre parametri del caricamento, ovvero:
- _load_paddr_: indirizzo fisico in memoria a cui caricare la pagina;
- _load_len_bytes_: numero di byte da leggere;
- _elf_offset_: offset della pagina all'interno del file eseguibile

si azzera la regione di memoria deputata ad ospitare la pagina, per poi effettuare la lettura, seguendo il pattern dato dalle operazioni:
- _uio_kinit_ per effettuare il setup delle strutture dati _uio_ e _iovec_
- _VOP_READ_ per effettuare l'operazione di lettura vera e propria

Vengono inoltre effettuati:
- controlli sul risultato dell'operazione di lettura, per ritornare al chiamante eventuali errori;
- verifica sul parametro _load_len_bytes_, per fini statistici: se esso è nullo, si registra un page fault relativo ad una pagina azzerata, altrimenti riguarda una pagina da caricare dall'eseguibile (e dal disco).

##### Operazioni di swapping
Si utilizzano le funzioni:
- _seg_swap_out_: segna come _swapped out_ la pagina corrispondente all'indirizzo virtuale passato, mediante l'analoga funzione del modulo _pt_; è invocata da _getppage_user_ all'interno del modulo _coremap_, il quale effettua fisicamente lo swap out del frame;
- _seg_swap_in_: effettua le operazioni di swap in del frame corrispondente all'indirizzo virtuale fornito, supponendo vero che la pagina fosse in stato di _swapped_:
    * ottiene l'offset nello swapfile, a partire dal contenuto della page table alla entry opportuna;
    * effettua fisicamente l'operazione di swap in del frame, utilizzando l'indirizzo fisico fornito;
    * utilizza l'analoga funzione del modulo _pt_ per inserire la corrispondenza (indirizzo virtuale, indirizzo fisico) nella entry opportuna.
 
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
Il Coremap è una componente fondamentale per la gestione della memoria fisica all'interno del sistema di memoria virtuale (VM). Questa struttura dati tiene traccia dello stato di ogni pagina di memoria fisica, consentendo al sistema di sapere quali pagine sono attualmente in uso, quali sono libere e quali devono essere sostituite o recuperate dal disco. Il Coremap gestisce sia le pagine utilizzate dal kernel che quelle utilizzate dai processi utente, facilitando la gestione dinamica della memoria in base alle esigenze del sistema.
#### Strutture dati
La struttura dati utilizzata per la gestione del Coremap è definita in coremap.h ed è la seguente:
```C
struct coremap_entry {
    coremap_entry_state entry_type; 
    unsigned long allocation_size;        
    
    unsigned long previous_allocated;   
    unsigned long next_allocated;   
    
    vaddr_t virtual_address;                  
    addrspace_t *address_space;                
};
```
Questa struttura serve a rappresentare lo stato e le proprietà di una singola pagina di memoria fisica. Ogni campo ha un ruolo specifico:
- _entry_type_: indica lo stato attuale della pagina, usando la enum coremap_entry_state. Può assumere lo stato:
  * COREMAP_BUSY_KERNEL: la pagina è allocata per l'uso del kernel.
  * COREMAP_BUSY_USER: la pagina è allocata per l'uso utente.
  * COREMAP_UNTRACKED: la pagina non è ancora gestita dal Coremap.
  * COREMAP_FREED: la pagina è stata liberata e può essere riutilizzata.

- _allocation_size_: specifica la dimensione dell'allocazione, ovvero il numero di pagine contigue allocate. Questo è particolarmente rilevante per le allocazioni del kernel, che possono richiedere blocchi di pagine contigue.

- _previous_allocated_ e _next_allocated_:  fungono da puntatori per una lista collegata delle pagine allocate. Vengono utilizzati per implementare una strategia FIFO (First-In-First-Out) per la sostituzione delle pagine, facilitando il tracking delle pagine in ordine di allocazione.

- _virtual_address_:  memorizza l'indirizzo virtuale associato alla pagina. È particolarmente importante per le pagine utente, dove il sistema deve mappare l'indirizzo virtuale dell'utente alla pagina fisica corrispondente.

- _address_space_:  punta allo spazio di indirizzamento (address space) a cui è allocata la pagina. È usato per identificare quale processo utente sta utilizzando la pagina.
#### Implementazione
L'implementazione del Coremap è fondamentale per la gestione della memoria all'interno del sistema operativo. I seguenti prototipi sono definiti per gestire l'inizializzazione, l'allocazione, la deallocazione e lo shutdown del Coremap:
```C
void coremap_init(void);            
void coremap_shutdown(void);        
vaddr_t alloc_kpages(unsigned npages); 
void free_kpages(vaddr_t addr);     
paddr_t alloc_user_page(vaddr_t vaddr); 
void free_user_page(paddr_t paddr); 
```
- _coremap_init()_: funzione che inizializza il Coremap, allocando la memoria necessaria per gestire tutte le pagine di memoria fisica disponibili. Imposta ogni entry inizialmente come COREMAP_UNTRACKED, indicando che le pagine non sono ancora gestite.

- _coremap_shutdown()_: funzione responsabile dell'arresto e della pulizia del Coremap, liberando la memoria allocata e disattivando il Coremap quando il sistema non ne ha più bisogno.

- _alloc_kpages(unsigned npages)_: funzione che gestisce l'allocazione delle pagine per il kernel. Tenta di allocare npages contigue, restituendo l'indirizzo virtuale del primo blocco. Se non ci sono pagine libere sufficienti, il sistema potrebbe tentare di "rubare" memoria (ram_stealmem).

- _free_kpages(vaddr_t addr)_: funzione che libera le pagine allocate al kernel, segnandole come COREMAP_FREED nel Coremap, rendendole disponibili per future allocazioni.

- _alloc_user_page(vaddr_t vaddr)_: funzione che gestisce l'allocazione delle pagine per i processi utente. Cerca prima di utilizzare pagine libere e, se necessario, sostituisce una pagina esistente usando una strategia di sostituzione FIFO. Se una pagina viene sostituita, la funzione interagisce con lo Swapfile per gestire il trasferimento della pagina al disco.

- _free_user_page(paddr_t paddr)_: funzione che libera le pagine allocate ai processi utente, rimuovendo la pagina dalla coda di allocazione e segnandola come COREMAP_FREED nel Coremap.
### Swapfile
Lo Swapfile è un componente essenziale per estendere la capacità di memoria fisica del sistema, permettendo al sistema operativo di gestire più processi di quanti possano essere contenuti nella memoria fisica disponibile. Quando la memoria RAM è piena, lo Swapfile permette di spostare temporaneamente le pagine su disco, liberando memoria per altri processi.
#### Implementazione
L'implementazione dello Swapfile prevede diverse funzioni per la gestione dello spazio di swap e il trasferimento delle pagine tra memoria fisica e disco. I prototipi delle funzioni principali sono:
```C
int swap_init(void);
int swap_out(paddr_t page_paddr, off_t *ret_offset);
int swap_in(paddr_t page_paddr, off_t swap_offset);
void swap_free(off_t swap_offset);
void swap_shutdown(void);
```
- _swap_init()_: funzione che inizializza lo Swapfile, creando il file di swap e la bitmap utile a  tracciare lo stato di utilizzo delle pagine nel file di swap.

- _swap_out(paddr_t page_paddr, off_t *ret_offset)_: funzione che scrive una pagina dalla memoria fisica al file di swap. Il parametro page_paddr indica l'indirizzo fisico della pagina da spostare su disco. La funzione restituisce l'offset nel file di swap dove la pagina è stata memorizzata, permettendo di recuperarla in seguito.

- _swap_in(paddr_t page_paddr, off_t swap_offset)_: funzione che legge una pagina dal file di swap e la ripristina nella memoria fisica. Il parametro swap_offset indica l'offset nel file di swap da cui recuperare la pagina.

- _swap_free(off_t swap_offset)_: funzione che libera lo spazio nel file di swap associato a una pagina che non è più necessaria. Il parametro swap_offset specifica l'offset della pagina nel file di swap da liberare.

- _swap_shutdown()_: funzione che chiude e libera le risorse associate allo Swapfile quando il sistema non ne ha più bisogno. Chiude il file di swap e rilascia la memoria utilizzata dalla bitmap.


## Test
Per testare il corretto funzionamento del sistema, abbiamo utilizzato i test già presenti all'interno di os161, scegliendo quelli adatti per ciò che è stato sviluppato:
- palin: effettua un semplice check su una stringa di 8000 caratteri, senza stressare la VM; non provoca replacements del TLB né swap in di pagine;
- matmult: effettua un prodotto matriciale (controllando il risultato ottenuto con quello atteso), occupando molto spazio in memoria e stressando maggiormente la VM rispetto al precedente;
- sort: ordina un array di grandi dimensioni usando l'algoritmo quick sort;
- zero: verifica che le aree di memoria da azzerare in allocazione siano correttamente azzerate (si ignora il controllo effettuato sulla syscall sbrk());
- faulter: verifica che l'accesso illegale ad un'area di memoria produca l'interruzione del programma;
- ctest: effettua l'attraversamento di una linked list;
