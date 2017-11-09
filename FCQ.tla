-------------------------------- MODULE FCQ --------------------------------
EXTENDS Integers, TLC
CONSTANT NR, NB

(* PlusCal options (-termination) *)

(*
--algorithm FCQ {
    variables readIdx = 0, readIdx_i = -1; 
              ReadIdxLock = -1; 
              rwIdxOffset = 0; rwIdxOffset_i = 0;
              buf = [i \in 0..NB |-> -1];
              buf_i = [i \in 0..NB |-> -1];
              csRdIdx = -1;
              
    macro CAS(result, val, expected, new) {
        if (val = expected) {
            val := new;
            result := TRUE;
        } else {
            result := FALSE;
        }
    }
    
    process(Reader \in 1..NR)
        variables cri=0, result = FALSE; 
    {
    r_mn: while(TRUE) {
            skip; \* NCS Stuff
   guard:   readIdx_i := readIdx; \* Split up the assignment for non-atomic assign
      r1:   cri := readIdx_i;
            if (cri # ReadIdxLock) {
      r2:       CAS(result, readIdx, cri, ReadIdxLock); \* Atomic compare & swap of readIdx
                if (result) {
                   \* Start of critical section      
                    csRdIdx := cri; \* for invariant checking
     cr1:           assert readIdx = ReadIdxLock;
                    if (rwIdxOffset < 1) {
                        readIdx := cri;
                        goto guard;
                    };
                    
     cr2:           assert buf[cri] = cri;
                    assert csRdIdx # writeIdx;
                    buf_i[cri] := -1;
     cr3:           buf[cri] := buf_i[cri];
                    assert buf[cri] = -1;
     cr5:           rwIdxOffset := rwIdxOffset - 1; \*Assumed to be atomic
                    readIdx_i := (cri + 1) % (NB + 1); \* Split up the assignment for non-atomic assign
     cr4:           readIdx := readIdx_i; 
                }
            }
          }  
    }
    
    process(Writer = NR+1)
      variable writeIdx = 0;
    {
      w_mn: while(TRUE) {
                skip;
        w1:     if (rwIdxOffset < NB) {
        w2:         assert buf[writeIdx] = -1;
                    buf_i[writeIdx] := writeIdx; \* Split up the assignment for non-atomic assign
        w3:         buf[writeIdx] := buf_i[writeIdx];
                    assert buf[writeIdx] = writeIdx;
                    writeIdx := (writeIdx + 1) % (NB + 1); \*Assume writeIdx update only happens in one thread
        w4:         rwIdxOffset := rwIdxOffset + 1; \*Assumed to be atomic
                }
            }
     }
            
}
*)
\* BEGIN TRANSLATION
VARIABLES readIdx, readIdx_i, ReadIdxLock, rwIdxOffset, rwIdxOffset_i, buf, 
          buf_i, csRdIdx, pc, cri, result, writeIdx

vars == << readIdx, readIdx_i, ReadIdxLock, rwIdxOffset, rwIdxOffset_i, buf, 
           buf_i, csRdIdx, pc, cri, result, writeIdx >>

ProcSet == (1..NR) \cup {NR+1}

Init == (* Global variables *)
        /\ readIdx = 0
        /\ readIdx_i = -1
        /\ ReadIdxLock = -1
        /\ rwIdxOffset = 0
        /\ rwIdxOffset_i = 0
        /\ buf = [i \in 0..NB |-> -1]
        /\ buf_i = [i \in 0..NB |-> -1]
        /\ csRdIdx = -1
        (* Process Reader *)
        /\ cri = [self \in 1..NR |-> 0]
        /\ result = [self \in 1..NR |-> FALSE]
        (* Process Writer *)
        /\ writeIdx = 0
        /\ pc = [self \in ProcSet |-> CASE self \in 1..NR -> "r_mn"
                                        [] self = NR+1 -> "w_mn"]

r_mn(self) == /\ pc[self] = "r_mn"
              /\ TRUE
              /\ pc' = [pc EXCEPT ![self] = "guard"]
              /\ UNCHANGED << readIdx, readIdx_i, ReadIdxLock, rwIdxOffset, 
                              rwIdxOffset_i, buf, buf_i, csRdIdx, cri, result, 
                              writeIdx >>

guard(self) == /\ pc[self] = "guard"
               /\ readIdx_i' = readIdx
               /\ pc' = [pc EXCEPT ![self] = "r1"]
               /\ UNCHANGED << readIdx, ReadIdxLock, rwIdxOffset, 
                               rwIdxOffset_i, buf, buf_i, csRdIdx, cri, result, 
                               writeIdx >>

r1(self) == /\ pc[self] = "r1"
            /\ cri' = [cri EXCEPT ![self] = readIdx_i]
            /\ IF cri'[self] # ReadIdxLock
                  THEN /\ pc' = [pc EXCEPT ![self] = "r2"]
                  ELSE /\ pc' = [pc EXCEPT ![self] = "r_mn"]
            /\ UNCHANGED << readIdx, readIdx_i, ReadIdxLock, rwIdxOffset, 
                            rwIdxOffset_i, buf, buf_i, csRdIdx, result, 
                            writeIdx >>

r2(self) == /\ pc[self] = "r2"
            /\ IF readIdx = cri[self]
                  THEN /\ readIdx' = ReadIdxLock
                       /\ result' = [result EXCEPT ![self] = TRUE]
                  ELSE /\ result' = [result EXCEPT ![self] = FALSE]
                       /\ UNCHANGED readIdx
            /\ IF result'[self]
                  THEN /\ csRdIdx' = cri[self]
                       /\ pc' = [pc EXCEPT ![self] = "cr1"]
                  ELSE /\ pc' = [pc EXCEPT ![self] = "r_mn"]
                       /\ UNCHANGED csRdIdx
            /\ UNCHANGED << readIdx_i, ReadIdxLock, rwIdxOffset, rwIdxOffset_i, 
                            buf, buf_i, cri, writeIdx >>

cr1(self) == /\ pc[self] = "cr1"
             /\ Assert(readIdx = ReadIdxLock, 
                       "Failure of assertion at line 37, column 21.")
             /\ IF rwIdxOffset < 1
                   THEN /\ readIdx' = cri[self]
                        /\ pc' = [pc EXCEPT ![self] = "guard"]
                   ELSE /\ pc' = [pc EXCEPT ![self] = "cr2"]
                        /\ UNCHANGED readIdx
             /\ UNCHANGED << readIdx_i, ReadIdxLock, rwIdxOffset, 
                             rwIdxOffset_i, buf, buf_i, csRdIdx, cri, result, 
                             writeIdx >>

cr2(self) == /\ pc[self] = "cr2"
             /\ Assert(buf[cri[self]] = cri[self], 
                       "Failure of assertion at line 43, column 21.")
             /\ Assert(csRdIdx # writeIdx, 
                       "Failure of assertion at line 44, column 21.")
             /\ buf_i' = [buf_i EXCEPT ![cri[self]] = -1]
             /\ pc' = [pc EXCEPT ![self] = "cr3"]
             /\ UNCHANGED << readIdx, readIdx_i, ReadIdxLock, rwIdxOffset, 
                             rwIdxOffset_i, buf, csRdIdx, cri, result, 
                             writeIdx >>

cr3(self) == /\ pc[self] = "cr3"
             /\ buf' = [buf EXCEPT ![cri[self]] = buf_i[cri[self]]]
             /\ Assert(buf'[cri[self]] = -1, 
                       "Failure of assertion at line 47, column 21.")
             /\ pc' = [pc EXCEPT ![self] = "cr5"]
             /\ UNCHANGED << readIdx, readIdx_i, ReadIdxLock, rwIdxOffset, 
                             rwIdxOffset_i, buf_i, csRdIdx, cri, result, 
                             writeIdx >>

cr5(self) == /\ pc[self] = "cr5"
             /\ rwIdxOffset' = rwIdxOffset - 1
             /\ readIdx_i' = (cri[self] + 1) % (NB + 1)
             /\ pc' = [pc EXCEPT ![self] = "cr4"]
             /\ UNCHANGED << readIdx, ReadIdxLock, rwIdxOffset_i, buf, buf_i, 
                             csRdIdx, cri, result, writeIdx >>

cr4(self) == /\ pc[self] = "cr4"
             /\ readIdx' = readIdx_i
             /\ pc' = [pc EXCEPT ![self] = "r_mn"]
             /\ UNCHANGED << readIdx_i, ReadIdxLock, rwIdxOffset, 
                             rwIdxOffset_i, buf, buf_i, csRdIdx, cri, result, 
                             writeIdx >>

Reader(self) == r_mn(self) \/ guard(self) \/ r1(self) \/ r2(self)
                   \/ cr1(self) \/ cr2(self) \/ cr3(self) \/ cr5(self)
                   \/ cr4(self)

w_mn == /\ pc[NR+1] = "w_mn"
        /\ TRUE
        /\ pc' = [pc EXCEPT ![NR+1] = "w1"]
        /\ UNCHANGED << readIdx, readIdx_i, ReadIdxLock, rwIdxOffset, 
                        rwIdxOffset_i, buf, buf_i, csRdIdx, cri, result, 
                        writeIdx >>

w1 == /\ pc[NR+1] = "w1"
      /\ IF rwIdxOffset < NB
            THEN /\ pc' = [pc EXCEPT ![NR+1] = "w2"]
            ELSE /\ pc' = [pc EXCEPT ![NR+1] = "w_mn"]
      /\ UNCHANGED << readIdx, readIdx_i, ReadIdxLock, rwIdxOffset, 
                      rwIdxOffset_i, buf, buf_i, csRdIdx, cri, result, 
                      writeIdx >>

w2 == /\ pc[NR+1] = "w2"
      /\ Assert(buf[writeIdx] = -1, 
                "Failure of assertion at line 62, column 21.")
      /\ buf_i' = [buf_i EXCEPT ![writeIdx] = writeIdx]
      /\ pc' = [pc EXCEPT ![NR+1] = "w3"]
      /\ UNCHANGED << readIdx, readIdx_i, ReadIdxLock, rwIdxOffset, 
                      rwIdxOffset_i, buf, csRdIdx, cri, result, writeIdx >>

w3 == /\ pc[NR+1] = "w3"
      /\ buf' = [buf EXCEPT ![writeIdx] = buf_i[writeIdx]]
      /\ Assert(buf'[writeIdx] = writeIdx, 
                "Failure of assertion at line 65, column 21.")
      /\ writeIdx' = (writeIdx + 1) % (NB + 1)
      /\ pc' = [pc EXCEPT ![NR+1] = "w4"]
      /\ UNCHANGED << readIdx, readIdx_i, ReadIdxLock, rwIdxOffset, 
                      rwIdxOffset_i, buf_i, csRdIdx, cri, result >>

w4 == /\ pc[NR+1] = "w4"
      /\ rwIdxOffset' = rwIdxOffset + 1
      /\ pc' = [pc EXCEPT ![NR+1] = "w_mn"]
      /\ UNCHANGED << readIdx, readIdx_i, ReadIdxLock, rwIdxOffset_i, buf, 
                      buf_i, csRdIdx, cri, result, writeIdx >>

Writer == w_mn \/ w1 \/ w2 \/ w3 \/ w4

Next == Writer
           \/ (\E self \in 1..NR: Reader(self))

Spec == /\ Init /\ [][Next]_vars
        /\ \A self \in 1..NR : WF_vars(Reader(self))
        /\ WF_vars(Writer)

\* END TRANSLATION

setMax(S) == CHOOSE t \in {S[x]:x \in 1..NR}: \As \in {S[x]:x \in 1..NR}: t >= s

(* Invariants *)
MutExTest == \A i, j \in 1..NR: (( j # i) => ~((pc[j] \in {"cr1", "cr2", "cr3", "cr4"}) /\ (pc[i] \in {"cr1", "cr2", "cr3", "cr4"})))

RWIdxTest == /\ (rwIdxOffset >= 0) 
             /\ (rwIdxOffset <= NB)
             
ReadIdxTest == ((readIdx >= 0) /\ (readIdx <= NB)) \/ (readIdx = ReadIdxLock)

\*Liveness == \E i \in 1..NR : pc[i] \notin {"main", "cs", "l3"} ~> \E k \in 1..N : pc[k] = "cs"



=============================================================================
\* Modification History
\* Last modified Thu Nov 09 11:14:24 PST 2017 by michael
\* Created Wed Oct 18 15:48:30 PDT 2017 by michael
