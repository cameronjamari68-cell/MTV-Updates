import random
import struct
from helios import overlay
from helios.controls import *


CREATIVE_BUTTON_OFFSET = 11
CREATIVE_LEGACY_STICK_OFFSET = 28
CREATIVE_PRECISION_STICK_OFFSET = 32
CREATIVE_PACKET_SIZE = 48


class CVWorker:
    def __init__(self, width, height):
        self.cvdata = bytearray(CREATIVE_PACKET_SIZE)
        self.noun = "user"
        self.nouns = ["user", "player", "member", "patron", "client", "consumer", "operator", "utilizer", "handler", "contender", "competitor", "contestant", "performer", "actor", "rival", "protagonist", "antagonist", "challenger", "adversary", "opponent", "friend", "teammate", "peer", "counterpart", "enemy"]
        self.frame_count = 0


    def process(self, frame):
        self.draw_menu_buttons()
        self.draw_face_buttons()
        self.draw_dpad()
        self.draw_triggers()
        self.draw_bumpers()
        self.draw_right_stick()
        self.draw_left_stick()

        overlay.text("Hello, " + str(self.noun), 790, 200, color=(255, 255, 255), scale=2, target=overlay.BOTH)
        if self.frame_count == 60:
            self.noun = self.nouns[random.randint(0, len(self.nouns)-1)]
            self.frame_count = 0
        overlay.text(str(self.frame_count), 10, 20, color=(255, 255, 255), scale=2, target=overlay.BOTH)
        self.frame_count += 1

        self.cvdata[0] = self.frame_count
        send_cvdata(self.cvdata)
    
    def draw_menu_buttons(self):
        buttons = {
            BUTTON_2: (250, 170),
            BUTTON_3: (320, 170)
        }
        pos_offset = (0, 50)
        for button, pos in buttons.items():
            value = get_actual(button)
            color = int(value * 2.55)
            self.write_button(button, value)
            x = pos[0] + pos_offset[0]
            y = pos[1] + pos_offset[1]
            overlay.circle(x, y, 12, color=(color, color, color), filled=True, target=overlay.BOTH)
            overlay.circle(x, y, 12, color=(255, 255, 255), thickness=3, target=overlay.BOTH)
    
    def draw_face_buttons(self):
        buttons = {
            BUTTON_14: (430, 140),
            BUTTON_15: (460, 170),
            BUTTON_16: (430, 200),
            BUTTON_17: (400, 170)
        }
        pos_offset = (0, 50)
        for button, pos in buttons.items():
            value = get_actual(button)
            color = int(value * 2.55)
            self.write_button(button, value)
            x = pos[0] + pos_offset[0]
            y = pos[1] + pos_offset[1]
            overlay.circle(x, y, 15, color=(color, color, color), filled=True, target=overlay.BOTH)
            overlay.circle(x, y, 15, color=(255, 255, 255), thickness=3, target=overlay.BOTH)
    
    def draw_dpad(self):
        dpad = {
            BUTTON_10: (190, 120),
            BUTTON_11: (190, 180),
            BUTTON_12: (160, 150),
            BUTTON_13: (220, 150)
        }
        pos_offset = (0, 150)
        for button, pos in dpad.items():
            value = get_actual(button)
            color = int(value * 2.55)
            self.write_button(button, value)
            x = pos[0] + pos_offset[0]
            y = pos[1] + pos_offset[1]
            overlay.rect(x, y, x + 27, y + 27, color=(color, color, color), filled=True, target=overlay.BOTH)
            overlay.rect(x, y, x + 27, y + 27, color=(255, 255, 255), thickness=3, target=overlay.BOTH)
    
    def draw_triggers(self):
        triggers = {
            BUTTON_8: (90, 260),
            BUTTON_5: (440, 260)
        }
        pos_offset = (0, 50)
        for button, pos in triggers.items():
            value = get_actual(button)
            color = int(value * 2.55)
            self.write_button(button, value)
            x = pos[0] + pos_offset[0]
            y = pos[1] + pos_offset[1] + int(value / 2)
            x2 = pos[0] + pos_offset[0] + 30
            y2 = pos[1] + pos_offset[1] + 50
            overlay.rect(x, y, x2, y2, color=(color, color, color), filled=True, target=overlay.BOTH)
            overlay.rect(x, y, x2, y2, color=(255, 255, 255), thickness=3, target=overlay.BOTH)
    
    def draw_bumpers(self):
        bumpers = {
            BUTTON_7: (95, 120),
            BUTTON_4: (395, 120)
        }
        pos_offset = (0, 0)
        for button, pos in bumpers.items():
            value = get_actual(button)
            color = int(value * 2.55)
            self.write_button(button, value)
            x = pos[0] + pos_offset[0]
            y = pos[1] + pos_offset[1]
            overlay.rect(x, y, x + 65, y + 20, color=(color, color, color), filled=True, target=overlay.BOTH)
            overlay.rect(x, y, x + 65, y + 20, color=(255, 255, 255), thickness=3, target=overlay.BOTH)
    
    def draw_left_stick(self):
        anchor = (130, 220)
        overlay.circle(anchor[0], anchor[1], 45, color=(255, 255, 255), thickness=2, target=overlay.BOTH)
        x_val = get_actual(STICK_2_X)
        y_val = get_actual(STICK_2_Y)
        click = get_actual(BUTTON_9)
        click_color = int(click * 2.55)
        self.write_button(BUTTON_9, click)
        x = int(anchor[0] + x_val*0.20)
        y = int(anchor[1] + y_val*0.20)
        overlay.circle(x, y, 37, color=(click_color, click_color, click_color), filled=True, target=overlay.BOTH)
        xy_color = int((abs(x_val) + abs(y_val)) * 2.25) + 30
        self.write_stick(2, x_val)
        self.write_stick(3, y_val)
        overlay.circle(x, y, 35, color=(xy_color, xy_color, xy_color), thickness=7, target=overlay.BOTH)

    def draw_right_stick(self):
        anchor = (360, 305)
        overlay.circle(anchor[0], anchor[1], 45, color=(255, 255, 255), thickness=2, target=overlay.BOTH)
        x_val = get_actual(STICK_1_X)
        y_val = get_actual(STICK_1_Y)
        click = get_actual(BUTTON_6)
        click_color = int(click * 2.55)
        self.write_button(BUTTON_6, click)
        x = int(anchor[0] + x_val*0.20)
        y = int(anchor[1] + y_val*0.20)
        overlay.circle(x, y, 37, color=(click_color, click_color, click_color), filled=True, target=overlay.BOTH)
        xy_color = int((abs(x_val) + abs(y_val)) * 2.25) + 30
        self.write_stick(0, x_val)
        self.write_stick(1, y_val)
        overlay.circle(x, y, 35, color=(xy_color, xy_color, xy_color), thickness=7, target=overlay.BOTH)

    def write_button(self, button, value):
        encoded = max(0, min(100, int(value))) | 1
        self.cvdata[CREATIVE_BUTTON_OFFSET + button] = encoded

    def write_stick(self, index, value):
        clamped = max(-100.0, min(100.0, float(value)))
        self.cvdata[CREATIVE_LEGACY_STICK_OFFSET + index] = (int(clamped) + 100) | 1
        struct.pack_into(">i", self.cvdata,
                         CREATIVE_PRECISION_STICK_OFFSET + index * 4,
                         int(clamped * 0x10000))
