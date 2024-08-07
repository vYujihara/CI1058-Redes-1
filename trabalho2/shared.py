MAX_UDP_SIZE = 65507

# Status dos jogadores
PLAYING = 1
ELIMINATED = 2 

# Endereços dos jogadores
PLAYERS_ADDR = [('10.254.224.60', 5010),  #i33, máquina 1
                ('10.254.224.61', 5011),  #i34, máquina 2
                ('10.254.224.58', 5012),   #i31, máquina 3
                ('10.254.224.59', 5013)]   #i32, máquina 4

# TIPOS DE MENSAGEM
TYPE_DEAL = 1
TYPE_GUESS = 2
TYPE_PLAY_CARD = 3
TYPE_REVEAL_SCORE = 4
TYPE_REVEAL_VIRA = 5
TYPE_REVEAL_GUESS = 6
TYPE_REVEAL_CARD = 7
TYPE_REVEAL_WINNER = 8
TYPE_NEW_ROUND = 9
TYPE_PASS_TOKEN = 10
TYPE_PLAY_CARD = 11
TYPE_COLLECT_STATUS = 12
TYPE_REVEAL_STATUS = 13
TYPE_MATCH_WINNER = 14


# Criação do baralho
DECK = []
suits = ['♦', '♠' , '♥', '♣']
faces = ['4', '5', '6', '7', '8', '9', 'Q', 'J', 'K', 'A', '2', '3']
for suit in suits:
    for face in faces:
        DECK.append((suit,face))