---
id: 2026-10-03-ottimizzazione-configurazione-master
stato: aperta
data-apertura: 2026-10-03
schema-version: 1
agente: claude
oggetto: configurazione
slice:
correlati: []
---

# Revisione: ottimizzazione della configurazione di Claude Code, suggerita dal master

## Richiesta
Il 2026-10-03 il master `_master-contracts` ha fatto una revisione della propria configurazione di Claude Code
con il comando `/doctor` (controllo di salute della configurazione: file di istruzioni, plugin, skill, server
MCP, permessi, hook) e ha applicato ciò che ne usciva. Quasi tutto riguardava la **macchina** (regole di permesso
globali, cache dei plugin, `CLAUDE.md` globale) ed è già risolto una volta per tutti i progetti. Quattro cose
invece sono **di progetto** e vanno valutate qui, nel repo: sono i *Rilievi* sotto.

Il revisore è il master (sessione di `_master-contracts`), i *Rilievi* sono il suo contributo; la *Risposta* la
scrive la sessione di questo progetto; chiude Marco. Sono **suggerimenti**, non obblighi né istruzioni: ogni
punto si accoglie, si respinge o si rimanda con il motivo. Nessun punto chiede di cancellare qualcosa senza
prima averlo visto: ogni spegnimento o rimozione è una decisione di Marco su un inventario mostrato prima.

Criteri di accettazione: ogni rilievo ha una risposta; nessuna modifica a file versionati senza il via di
Marco; le stime di token sono dichiarate come stime (caratteri diviso 4).

## Rilievi

### Standard — rispetto delle regole del repo
1. [media] `CLAUDE.md` del progetto — il file si carica a ogni sessione e si paga ogni volta. Nel master
   era per il 55% copia di cose che vivono altrove: elenco di progetti o tabelle tenute in un registro,
   storia delle revisioni del contratto (vive nel `CHANGELOG`), albero delle cartelle (si vede con `ls`),
   elenco delle skill con descrizione (le `description` sono già nell'elenco delle skill). Tolte le copie
   e tenuto solo ciò che non si ricava dal repo, il file è sceso del 36%. Si suggerisce la stessa lettura
   qui: per ogni sezione, «si ricava dal repo o da un file già in contesto?». Le eccezioni dichiarate, la
   politica git e i divieti restano sempre.
2. [bassa] `.claude/settings.json` del progetto, `permissions.allow` — se esiste, rileggere le regole
   preapprovate: in modalità automatica una regola `allow` per un comando preciso viene applicata **prima**
   del classificatore (documentazione ufficiale, pagina `auto-mode-config`: «narrow Bash and PowerShell allow
   rules … Claude Code resolves them before the classifier runs»), quindi una regola larga (`Bash(gh pr *)`,
   `Bash(git push *)`, uno strumento di rilascio con `*`) scavalca ogni controllo. Nel master ne sono state
   trovate otto nel file globale; qui il controllo è sul file di progetto.

### Specifica — cosa resta da valutare in questo progetto
1. [media] Plugin e server MCP che qui non servono — nel master sono stati spenti **solo per quel progetto**
   (chiavi `"<nome>@<marketplace>": false` in `enabledPlugins` di `.claude/settings.json`; per i server MCP
   utente e i connettori claude.ai, `disabledMcpServers` nella voce del progetto in `~/.claude.json`, lo
   stesso campo che scrive il menu `/mcp`). Lo stack di questo progetto dice quali plugin sono inutili (per
   esempio i tre plugin Shopify fuori dai progetti Shopify, `frontend-design` fuori dai progetti con UI).
   L'inventario con i conteggi d'uso lo dà `/doctor` nella sessione del progetto.
2. [bassa] Skill del kit irrilevanti per questo stack — `stile-frontend` dove non c'è UI, `verify` dove non
   c'è build: si possono spegnere solo qui con `"skillOverrides": {"<skill>": "off"}` in
   `.claude/settings.local.json` (file ignorato da git), lasciando i file per la propagazione. Circa 150
   token stimati a skill, a ogni sessione.
3. [bassa] Invito — lanciare `/doctor` nella sessione di questo progetto e decidere da sé sul proprio
   inventario: è un controllo in sola lettura che riferisce e chiede prima di cambiare qualunque cosa. Due
   proposte che potrebbe fare e che richiedono cautela: la pulizia della cache dei plugin è di macchina (già
   fatta dal master il 2026-10-03, 360 MB di cloni temporanei non referenziati: non c'è più nulla da
   cancellare) e ogni cancellazione passa dall'inventario completo mostrato a Marco.

## Risposta
(Claude) 1. Accolto: corretto in <commit/file>. | Respinto: <motivo verificabile>. | Rimandato: `TODO(review): …` in <file>.

## Replica
Il master (`_master-contracts`), 2026-10-07, su decisione di Marco. Non rilievi nuovi, ma il **modo più economico di
rispondere a quelli sopra**: Claude Code ha diversi comandi di autocontrollo della configurazione, e il master li
ha passati in rassegna tutti sulla documentazione ufficiale (`code.claude.com/docs/en/commands`, verificata il
2026-10-07). Ne valgono **cinque**, in quest'ordine; il resto è di macchina (classificatore dell'auto mode, cache
dei plugin, già fatto una volta per tutti) o serve solo a chi scrive skill. Tutti riferiscono e chiedono prima di
cambiare qualcosa; ogni spegnimento o rimozione resta una decisione di Marco su un inventario mostrato prima.

**Dove si lanciano.** Dall'estensione VS Code si dà solo `/doctor` senza argomenti (verificato da Marco il
2026-10-07): i comandi con argomento, e probabilmente anche gli altri, vanno dati dal **terminale**, con `claude`
aperto nella cartella del progetto. Versioni minime dalla documentazione: `/doctor prompt-audit` 2.1.283,
`/skill-doctor` 2.1.252.

1. **`/doctor`** — una volta, l'ombrello: impostazioni illeggibili, skill, plugin e server MCP inutilizzati con il
   loro costo, hook lenti. Dalla 2.1.205 **può anche correggere**, chiedendo conferma: rispondere no a ogni
   correzione automatica e portare l'esito nella *Risposta* di questa revisione. Si decide: quali plugin e MCP
   spegnere **solo qui** (`enabledPlugins` in `.claude/settings.json`, `disabledMcpServers` nella voce del progetto
   in `~/.claude.json`).
2. **`/doctor prompt-audit`** (terminale) — il più utile per un satellite: legge `CLAUDE.md`, la copia del
   contratto, regole e skill e segnala istruzioni scritte per modelli vecchi, rimandi a file o comandi che non
   esistono, contraddizioni fra file. Propone, non applica. Si decide: cosa correggere nel per-repo (le modifiche
   alla copia del contratto **non** si fanno qui: si segnalano al master, che le porta nel prossimo sync).
3. **`/context`** — quando la sessione sembra pesante: cosa occupa il contesto per categoria e quali file di
   istruzioni sono caricati. Solo lettura. Si decide: se il per-repo o il contratto pesano troppo, cosa è ricavabile
   dal repo e può uscire.
4. **`/skill-doctor`** — costo in contesto e frequenza d'uso di ogni skill, skill mai invocate. Solo lettura. Si
   decide: quali skill del kit spegnere **solo qui** con `"skillOverrides": {"<skill>": "off"}` in
   `.claude/settings.local.json` (file ignorato da git), lasciando i file per la propagazione.
5. **`/insights`** — una volta al mese: rapporto HTML sulle ultime sessioni, punti di attrito, errori ricorrenti,
   funzioni non usate. Scrive il file del rapporto (percorso non confermato dalla documentazione), non tocca il
   progetto. Non disponibile nelle sessioni in cloud.

Esclusi di proposito: `/fewer-permission-prompts` (propone regole `allow` larghe, in contrasto con il rilievo
Standard 2), `claude plugin validate` ed `eval` (per chi scrive skill, cioè il master), `claude auto-mode *`
(configurazione di macchina, già fatta).

Per rispondere basta la *Risposta* di questa revisione: per ciascuno dei cinque, lanciato o no, e cosa si è deciso.
Chiude Marco.

## Esito
(solo l'utente) chiusa il <data>: <cosa è stato fatto>.
