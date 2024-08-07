import sys
from ring import token_ring
from player import *
from shared import *

assert len(sys.argv) == 2, "Apenas um argumento a ser utilizado"

player_n = int(sys.argv[1]) - 1
p_addr = PLAYERS_ADDR[player_n]

next_p_addr = PLAYERS_ADDR[(player_n + 1) % 4]

player = Player(p_addr, next_p_addr)


if player_n == 0:
    player.token = 1
    answer = ''
    while answer != 'Y':
      answer = input("Para começar a partida digite 'Y': \n")  
token_ring(player)