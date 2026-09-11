# RAW hazards: SUB needs R1 from ADD; MUL needs R6 from LOAD (load-use hazard).
ADD R1, R2, R3
SUB R4, R1, R5
LOAD R6, 0(R2)
MUL R7, R6, R1
