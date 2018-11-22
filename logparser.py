import numpy as np

STATE_MAP = {'#':0, 'L':1, 'T':2, 'W':3, 'A':4, 'F':5, 'I':6, 'C':7}
CHAIN_MAP = {
    'LUXOR':0, 0:'LUXOR',
    'TOWER':1, 1:'TOWER',
    'WORLDWIDE':2, 2:'WORLDWIDE',
    'AMERICAN':3, 3:'AMERICAN',
    'FESTIVAL':4, 4:'FESTIVAL',
    'IMPERIAL':5, 5:'IMPERIAL',
    'CONTINENTAL':6, 6:'CONTINENTAL'}

def parse_log(path):
    with open(path,'r') as f:
        file_contents = f.read()
    contents = file_contents.splitlines()

    turn_starts = [i for i,line in enumerate(contents) if line == 'TURN START']
    turn_starts = turn_starts + [i for i,line in enumerate(contents) if line == 'runGame: END OF GAME']
    
    game_log = {}
    game_log['start'] = parse_header(contents[:turn_starts[0]])
    turns = []
    for turn_id in range(len(turn_starts)-1):
        print('TURNID:',turn_id)
        action, state = parse_turn(contents[turn_starts[turn_id]:turn_starts[turn_id+1]])
        turns.append((action,state))
    game_log['turns'] = turns
    
    game_log['end'] = parse_end_state(contents[turn_starts[-1]:])
    return game_log

def parse_header(contents):
    header = {}
    header['player start'] = contents[0].split(':')[2]
    state = {}
    state['board'] = parse_board(contents[1:10])
    state = add_players_to_state(state, contents, 11)
    return header, state

def parse_state(contents):
    board_start = [i+1 for i,line in enumerate(contents) if 'canEndGame' in line][0]
    board_end = board_start + 9
    state = {}
    state['board'] = parse_board(contents[board_start:board_end])
    state = add_players_to_state(state, contents, board_end+1)
    return state 
    
def parse_board(contents):
    board = np.zeros((8,9,12), np.uint8)
    for i,line in enumerate(contents):
        states = line.split(' ')
        for j, state in enumerate(states[:-1]):
            if state != '.':
                board[STATE_MAP[state], i, j] = 1
    return board

def parse_player(contents):
    player = {}
    player['name'] = contents[0][10:-1]
    player['cash'] = int(contents[1][1:])
    player['tiles'] = [int(i) for i in contents[2].strip().split(' ')[1:]]
    player['stocks'] = [int(i) for i in contents[3].strip().split(' ')[1:]]
    player['value'] = int(contents[4].split('$')[1])
    return player

def add_players_to_state(state, contents, start):
    end = start+5
    for player_id in range(4):
        state[player_id] = parse_player(contents[start:end])
        start = end
        end = start + 5
    return state


def parse_actions(contents):
    actions = {}
    # tile playing
    playing_lines = [line for line in contents if 'handleTilePlayPhase' in line]
    for line in playing_lines:
        if 'Player Number' in line:
            actions['player'] = int(line.split(':')[-1])
        if 'Playing tile' in line:
            actions['tile'] = int(line.split(':')[-1])
    
    # logging merge choices    
    merger_lines = [line for line in contents if 'handleTilePlayMerger' in line]
    if len(merger_lines) > 0:
        merge_action = {}
        merge_chains = []
        for line in merger_lines:
            if 'Merging Chains' in line:
                merge_chains.append(CHAIN_MAP[line.split(':')[-1].strip()])
            if 'Surviving chain' in line:
                survivor = CHAIN_MAP[line.split(':')[-1].strip()]
        merge_trade_index = [i for i,line in enumerate(contents) if 'Player Merge Actions:' in line]
        player_actions = []
        for start_index in merge_trade_index:
            player = {}
            player['player'] = int(contents[start_index].split(':')[-1])
            player['sell'] = int(contents[start_index+1].split(':')[-1])
            player['trade'] = int(contents[start_index+2].split(':')[-1])
            player_actions.append(player)
        merge_actions = {'chains':merge_chains, 'survivor':survivor, 'player_actions': player_actions}
        actions['merge'] = merge_actions

    # logging chain creation
    create_lines = [line for line in contents if 'Create' in line]
    if len(create_lines) > 0:
        chain = CHAIN_MAP[create_lines[0].split(':')[-1].strip()]
        if len(create_lines) == 2:
            share = True
        else:
            share = False
        create_action = {'chain':chain, 'share':share}
        actions['create'] = create_action

    # logging share purchases
    share_purchase_lines = [line for line in contents if 'SharePurchasePlan' in line]
    if len(share_purchase_lines) > 0:
        share_actions = {}
        for line in share_purchase_lines:
            tokens = line.split(':')
            share_actions[CHAIN_MAP[token[1].strip()]] = int(token[2]) 
        actions['share'] = share_actions
    return actions 

def parse_turn(contents):
    actions = parse_actions(contents)
    state = parse_state(contents)
    return actions, state
    
def parse_end_state(contents):
    state = {}
    state['winner'] = int(contents[1].split(':')[-1])
    state['value'] = int(contents[2].split(':')[-1])
    board_start = 3
    board_end = board_start + 9
    state['board'] = parse_board(contents[board_start:board_end])
    state = add_players_to_state(state, contents, board_end+1)
    return state

if __name__ == '__main__':
    game_log = parse_log('../tmp/0.txt')
    print(game_log['start'])
    for turn in game_log['turns']:
        print(turn[0])

    print(game_log['end'])
    
