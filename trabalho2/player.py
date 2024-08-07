import pickle
import socket
from shared import *
import random
import copy
from message import Message

class Player:
    def __init__(self, p_addr, next_p_addr):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(p_addr)

        self.p_addr = p_addr                                       # Endereço do jogardor
        self.next_p_addr = next_p_addr                             # Endereço do próximo jogador
        self.life = 5                                              # Vida do jogador
        self.token = 0                                             # Posse do bastão     
        self.hand = []                                             # Conjunto de cartas
        self.status = PLAYING                                      # Status do jogardor
        self.round = 1                                             # Representa a partida, número de cartas
        self.vira = 0                                              # Armazena o vira da partida
        self.players_guesses = []                                  # Variavel para carteador armazenar palpites
        self.players_status = {1:True, 2:True, 3:True, 4:True}     # Status de todos os players

    # Checa o endreço da origem
    def check_message(self, addr):
        if addr in PLAYERS_ADDR:
            return
        else:
            print("Endereço desonhecido")
            exit(1)

    # Carteador checa se a mensagem é a que enviou
    def check_source(self, addr):
        if addr != self.p_addr:
            print("Endereço de destino não correspondente")
            exit(1)
        

    # Quantidadade de players ainda jogando
    def get_total_players(self):
        count = 0
        for value in self.players_status.values():
            if(value):
                count+=1
        return count
    
    # Index do player
    def get_player_index(self, addr):
        return PLAYERS_ADDR.index(addr)
    
    # Transforma message no formato de string e envia
    def send_message(self, message):
        data_string = pickle.dumps(message)
        self.socket.sendto(data_string, self.next_p_addr)

    # Manda mensagem onde todos os players jogando escuta
    def reveal(self, data , type):
        message = Message(self.p_addr, 0, type, data, [])
        self.send_message(message)

        while True:
            data_recv, addr = self.socket.recvfrom(MAX_UDP_SIZE)
            data_var: Message = pickle.loads(data_recv)
            read = 0
            self.check_source(data_var.source)
            # Checa se todos os players ainda jogando receberam
            for flag in data_var.read:
                if(flag):
                    read += 1
            if(read != self.get_total_players() - 1):
                self.send_message(message)
            else:
                break
        
    # Mostra as cartas da mão e o vira
    def print_cards(self):
        print(f'Vira: {self.vira[1]}{self.vira[0]}')
        print('Cartas:', end="")
        for index, carta in enumerate(self.hand):
            print(f' {index + 1}:[{carta[1]}{carta[0]}] ', end="")
        print("")

    # Distribui as cartas dos jogadores e revela o vira
    def deal_cards(self):
        deck = copy.deepcopy(DECK)
        random.shuffle(deck)

        # Volta para 1 carta se o braalho não é suficiente
        total_cards = self.round * self.get_total_players()
        if total_cards >= len(deck):
            self.round = 1
            self.reveal(self.round, TYPE_NEW_ROUND)

        # Retira as carta do baralho e distribui
        for index, value in enumerate(self.players_status.values()):
            if(value):
                data = []
                for i in range(self.round):
                    data.append(deck.pop())
                if(self.p_addr == PLAYERS_ADDR[index]):
                    self.hand = data
                    print('Recebeu:', end="")
                    for carta in self.hand:
                        print(f' [{carta[1]}{carta[0]}]',end="")
                    print("")
                    print(f'Vidas: {self.life}')
                else:
                    message = Message(self.p_addr, PLAYERS_ADDR[index], TYPE_DEAL, data, 0)
                    self.send_message(message)
                    while(True):
                        data_recv, addr = self.socket.recvfrom(MAX_UDP_SIZE)
                        data_var: Message = pickle.loads(data_recv)
                        self.check_source(data_var.source)
                        if(data_var.type == TYPE_DEAL and self.token):
                            if(not data_var.read):
                                self.send_message(message)
                            else:
                                break
        vira = deck.pop()
        self.vira = vira
        print(f'O vira da rodada é: [{self.vira[1]}{self.vira[0]}]')
        self.reveal(vira, TYPE_REVEAL_VIRA)

    # Carteador coleta os palpites e revela para os players
    def collect_guesses(self):

        # Recolhe os palpites, carteador da palpite por último
        p_index = self.get_player_index(self.p_addr)
        n_guesses = 0
        guesses = []

        for i in range(p_index + 1, p_index + 5):
            guess_index = ( i % 4 )
            if self.p_addr == PLAYERS_ADDR[guess_index]:
                guess = input('Digite seu palpite com um número: ')
                while True:
                    if int(guess) + n_guesses == self.round:
                        guess = input('O número de palpites não pode igualar o número de cartas: ')
                    elif int(guess) < 0:
                        guess = input('Número do palpite não pode ser negativo: ')
                    else:
                        break
                    print(f'p{self.get_player_index(self.p_addr) + 1} faz {guess}!')

                aux = (self.p_addr,int(guess))
                self.reveal(aux, TYPE_REVEAL_GUESS)
                print(f'p{self.get_player_index(self.p_addr) + 1} faz {data_var.data[1]}!')
                guesses.append(aux)
                self.players_guesses = guesses

            elif self.players_status[guess_index + 1]:
                message = Message(self.p_addr, PLAYERS_ADDR[guess_index], TYPE_GUESS, 0, 0)
                self.send_message(message)
                while(True):
                        data_recv, addr = self.socket.recvfrom(MAX_UDP_SIZE)
                        data_var: Message = pickle.loads(data_recv)
                        self.check_source(data_var.source)
                        if(not data_var.read):
                           self.send_message(message)
                        else:
                            guess = data_var.data
                            n_guesses += guess[1]
                            guesses.append(guess)
                            print(f'p{guess_index + 1} faz {data_var.data[1]}!')
                            self.reveal(guess, TYPE_REVEAL_GUESS)
                        break

    # Carteador manda mensagem para cada jogador escolher uma carta para jogar
    def card_play(self, first_player, cards_played):
        index = self.get_player_index(first_player)

        for i in range(index , index + 4):
            p_index = ( i % 4 )
            # Se é a vez do carteador
            if(self.p_addr == PLAYERS_ADDR[p_index]):
                self.print_cards()
                card_index = int(input('Digite o número correspondente da carta que quer jogar: ')) - 1
                while card_index < 0 or card_index >= len(self.hand):
                        card_index = int(input('Número invalido, escolha novamente: ')) - 1
                card = self.hand[card_index]
                self.hand.pop(card_index)
                player_card = (self.p_addr, card)
                cards_played.append(player_card)
                print(f'O jogador p{p_index + 1} jogou a carta {player_card[1][1]}{player_card[1][0]}')
                self.reveal(player_card, TYPE_REVEAL_CARD)
            elif self.players_status[p_index + 1]:
                message = Message(self.p_addr, PLAYERS_ADDR[p_index], TYPE_PLAY_CARD, 0, 0)
                self.send_message(message)
                while(True):
                        data_recv, addr = self.socket.recvfrom(MAX_UDP_SIZE)
                        data_var: Message = pickle.loads(data_recv)
                        self.check_source(data_var.source)
                        if(not data_var.read):
                           self.send_message(message)
                        else:
                            player_card = (data_var.data)
                            cards_played.append(player_card)
                            print(f'O jogador p{p_index + 1} jogou a carta {player_card[1][1]}{player_card[1][0]}')            
                            self.reveal(player_card, TYPE_REVEAL_CARD)
                            break
        
    # Função para resolver as cartas jogadas da rodada
    def resolve_round(self, cards_played):
        manilhas = []
        n_manilha = faces.index(self.vira[1])
        n_manilha = (n_manilha + 1) % 12

        winner = 0
        for card in cards_played:
            if card[1][1] == faces[n_manilha]:
                manilhas.append(card)
        if len(manilhas) == 1:
            winner = manilhas[0]

        elif len(manilhas) > 1:
            while manilhas:
                card = manilhas.pop()
                if not winner:
                    winner = card
                else:
                    if(suits.index(card[1][0]) > suits.index(winner[1][0])):
                        winner = card
        else:
            while True:
                count = 0
                for card in cards_played:
                    if not winner:
                        winner = card
                    else:
                        if faces.index(card[1][1]) > faces.index(winner[1][1]):
                            winner = card
                            count = 0
                        elif faces.index(card[1][1]) == faces.index(winner[1][1]):
                            count += 1
                if count > 0:
                    cards_played = [card for card in cards_played if card[1][1] != winner[1][1]]
                    winner = 0
                elif count == 0:
                    break
                if len(cards_played) == 0:
                    winner = 0
                    break
        
        if not winner:
            print('Resultado: Sem ganhador na rodada!\n')
            self.reveal(winner, TYPE_REVEAL_WINNER)
            return 0
        else:
            p_index = self.get_player_index(winner[0]) + 1
            print(f'Resultado: O jogador p{p_index} ganhou a rodada\n')
            self.reveal(winner, TYPE_REVEAL_WINNER)
            return winner[0]
    
    # Carteador manda os pontos da rodada para todos os jogadores
    def send_score(self, scores):
        lifes = [0, 0, 0, 0]

        for guess in self.players_guesses:
            p_index = self.get_player_index(guess[0])
            life = scores[p_index] - guess[1]
            if life < 0:
                life = life * (-1)
            lifes[p_index] = life
        self.reveal(lifes, TYPE_REVEAL_SCORE)
        self_index = self.get_player_index(self.p_addr)
        self.life -= lifes[self_index]

        for i, status in enumerate(self.players_status):
            if status:
                if lifes[i]:
                    print(f'O jogador p{i + 1} perdeu {lifes[i]} vida')
                else:
                    print(f'O jogador p{i + 1} não perdeu vida')
        print("")
    
    # Carteador anuncia o vencedor do jogo
    def send_match_winner(self, winner):
        message = Message(self.p_addr, 0, TYPE_MATCH_WINNER, winner, [])
        self.send_message(message)
        while(True):
            data_recv, addr = self.socket.recvfrom(MAX_UDP_SIZE)
            data_var: Message = pickle.loads(data_recv)
            self.check_source(data_var.source)
            read = 0
            for flag in data_var.read:
                if(flag):
                    read += 1
            if(read != 3):
                self.send_message(message)
            else:
                break
    
    # Função para definir quem continua jogando ou não
    def resolve_status(self):
        message = Message(self.p_addr, 0, TYPE_COLLECT_STATUS, [], [])
        self.send_message(message)

        while(True):
            data_recv, addr = self.socket.recvfrom(MAX_UDP_SIZE)
            data_var: Message = pickle.loads(data_recv)
            self.check_source(data_var.source)
            read = 0
            for flag in data_var.read:
                if(flag):
                    read += 1
            if(read != self.get_total_players() - 1):
                self.send_message(message)
            else:
                break
        status = data_var.data
        index = self.get_player_index(self.p_addr) + 1
        if self.life <= 0:
            aux = (self.p_addr, self.life)
            status.append(aux)
            self.players_status[index] = False
        if not self.players_status[index]:
            print('Você foi eliminado!')
        else:
            for p_addr in status:
                p_index = self.get_player_index(p_addr[0])
                print(f'O jogador p{p_index + 1} foi eliminado!')
                self.players_status[p_index + 1] = False
        total_players = self.get_total_players()
        winner = 0
        if total_players == 1:
            for key, value in self.players_status.items():
                if value:
                    winner = key
        count = 0
        if not total_players:
            for i, p in enumerate(status):
                if i == 0:
                    winner = p
                else:
                    if p[1] > winner[1]:
                        winner = p
                        count = 0
                    if p[1] == winner[1]:
                        count += 1
            aux = winner
            winner = self.get_player_index(winner[0]) + 1
        
        if count:
            tie = []
            print('Os jogadores', end="")
            for p in status:
                if p[1] == aux[1]:
                    tie.append(p[0])
                    print(f' p{self.get_player_index(p[0]) + 1}', end = "")
            print(' empataram!')
            self.send_match_winner(tie)
            exit(1)
        if winner:
            print(f'O jogador p{winner} é o vencedor!')
            self.send_match_winner(winner)
            exit(1)
        else:
            self.reveal(status, TYPE_REVEAL_STATUS)
        
    # Passa o bastão para o próximo jogador ativo
    def pass_token(self):
        next_player = self.get_player_index(self.next_p_addr)
        while True:
            if self.players_status[next_player + 1]:
                break
            next_player = (next_player + 1) % 4
        next_player = PLAYERS_ADDR[next_player]
        message = Message(self.p_addr, next_player, TYPE_PASS_TOKEN, 0, 0)
        self.send_message(message)
        self.token = 0