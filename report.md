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

### Page table

### Coremap

### Swapfile
