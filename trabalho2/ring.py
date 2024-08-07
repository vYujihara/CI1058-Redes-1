import socket, pickle
from shared import *
from message import Message
from player import *


def token_ring(player:Player):
    while True:
        if player.token:
                print('Você é o carteador da rodada\n')
                player.deal_cards()
                player.collect_guesses()
                scores = [0, 0, 0, 0]
                # Jogada das cartas
                for i in range(player.round):
                    cards_played = []
                    if (i == 0):
                        first_player = player.next_p_addr
                        player.card_play(first_player, cards_played)
                        last_winner = first_player
                    else:
                        player.card_play(winner, cards_played)
                    winner = player.resolve_round(cards_played)
                    if winner:
                        scores[player.get_player_index(winner)] += 1
                        last_winner = winner
                    # Se a rodada resultar em empate, o ganhador da rodada anterior começa
                    else:
                        winner = last_winner
                player.send_score(scores)
                player.resolve_status()
                player.round += 1
                player.reveal(player.round, TYPE_NEW_ROUND)
                player.pass_token()

        data, addr = player.socket.recvfrom(MAX_UDP_SIZE)
        data_var: Message = pickle.loads(data)
        player.check_message(data_var.source)

        # Recebimento das mensagem

        # Revela o vencedor da partida 
        if data_var.type == TYPE_MATCH_WINNER:
            if isinstance(data_var.data, int):
                print(f'O jogador p{data_var.data} é o vencedor!')
            else:
                print('Os jogadores', end="")
                for p in data_var.data:
                    print(f' p{player.get_player_index(p) + 1}', end="")
                print(' empataram!')
            data_var.read.append(1)
            player.send_message(data_var)
            exit(1)

        # Se eliminado, manda a mensagem para o próximo
        elif player.status == ELIMINATED:
            player.send_message(data_var)


        # Recebe as cartas e mostra na tela as cartas e vida
        elif data_var.type == TYPE_DEAL:
            if data_var.dest == player.p_addr:
                player.hand = data_var.data
                print('Recebeu:', end="")
                for carta in player.hand:
                    print(f' [{carta[1]}{carta[0]}]', end="")
                print("")
                print(f'Vidas: {player.life}')
                data_var.read = 1
            player.send_message(data_var)

        # Mostra na tela para dar o palpite
        elif data_var.type == TYPE_GUESS:
            if data_var.dest == player.p_addr:
                guess = input('Digite seu palpite com o número: ')
                while int(guess) < 0:
                    guess = input('Número do palpite não pode ser negativo: ')
                guess_data = (player.p_addr, int(guess))
                data_var.data = guess_data
                data_var.read = 1
            player.send_message(data_var)

        # Mostra na tela o palpite de um jogador
        elif data_var.type ==  TYPE_REVEAL_GUESS:
            p_index = player.get_player_index(data_var.data[0]) + 1
            print(f'p{p_index} faz {data_var.data[1]}!')
            data_var.read.append(1)
            #print(f'read em reveal guess {data_var.read}')
            player.send_message(data_var)

        # Mostra na tela o vira da rodada
        elif data_var.type == TYPE_REVEAL_VIRA:
            player.vira = data_var.data
            print(f'O vira da rodada é: [{player.vira[1]}{player.vira[0]}]')
            data_var.read.append(1)
            player.send_message(data_var)

        # Escolhe a carta que quer jogar e coloca no campo data
        elif data_var.type == TYPE_PLAY_CARD:
            if data_var.dest == player.p_addr:  
                player.print_cards()
                card_index = int(input('Digite o número correspondente da carta que quer jogar: ')) - 1
                while card_index < 0 or card_index >= len(player.hand):
                    card_index = int(input('Número invalido, escolha novamente: ')) - 1
                card = player.hand[card_index]
                player.hand.pop(card_index)
                player_card = (player.p_addr, card)
                data_var.data = player_card
                data_var.read = 1
            player.send_message(data_var)

        # Informa a carta jogada
        elif data_var.type == TYPE_REVEAL_CARD:
            p_index = player.get_player_index(data_var.data[0]) + 1
            print(f'O jogador p{p_index} jogou a carta {data_var.data[1][1]}{data_var.data[1][0]}')
            data_var.read.append(1)
            player.send_message(data_var)

        # Mostra na tela o ganhador da rodada
        elif data_var.type == TYPE_REVEAL_WINNER:
            if not data_var.data:
                print('Resultado: Sem ganhador na rodada!\n')
            else:
                p_index = player.get_player_index(data_var.data[0]) + 1
                print(f'Resultado: O jogador p{p_index} ganhou a rodada\n')
            data_var.read.append(1)
            player.send_message(data_var)

        # No final da rodada, mostra na tela se os jogadores perderam vida
        elif data_var.type == TYPE_REVEAL_SCORE:
            lifes = data_var.data
            for i, status in enumerate(player.players_status):
                if status:
                    if lifes[i]:
                        print(f'O jogador p{i + 1} perdeu {lifes[i]} vida')
                    else:
                        print(f'O jogador p{i + 1} não perdeu vida')
            print("")
            life = data_var.data[player.get_player_index(player.p_addr)]
            player.life -= life
            data_var.read.append(1)
            player.send_message(data_var)

        # Informa o número da rodada para os players
        elif data_var.type == TYPE_NEW_ROUND:
            player.round = data_var.data
            data_var.read.append(1)
            player.send_message(data_var)

        # Envia o status no campo data se foi eliminado
        elif data_var.type == TYPE_COLLECT_STATUS:
            if player.life <= 0:
                status = (player.p_addr, player.life)
                player.status = ELIMINATED
                print('Você foi eliminado!')
                data_var.data.append(status)
            data_var.read.append(1)
            player.send_message(data_var)

        # Revela para os jogadores se um jogador foi eliminado
        elif data_var.type == TYPE_REVEAL_STATUS:
            status = data_var.data
            for p in status:
                p_index = player.get_player_index(p[0])
                print(f'O jogador p{p_index + 1} foi eliminado!')
                player.players_status[p_index + 1] = False
            
            data_var.read.append(1)
            player.send_message(data_var)

        # Recebe o token se for o destino
        elif data_var.type == TYPE_PASS_TOKEN:
            if data_var.dest == player.p_addr:
                data_var.read = 1
                player.token = 1
                continue
        else:
            print('Mensagem de outro pessoa')
            exit(1)