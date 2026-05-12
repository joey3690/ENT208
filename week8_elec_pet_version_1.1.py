from m5stack import *
from m5ui import *
from uiflow import *
import imu        
import time
import random
import math

# 初始化屏幕和传感器
setScreenColor(0x000000)
imu0 = imu.IMU()

def draw_idle_face():
    """绘制基础的平淡表情"""
    lcd.clear()
    lcd.circle(85, 105, 25, fillcolor=0xffffff, color=0xffffff)
    lcd.circle(235, 105, 25, fillcolor=0xffffff, color=0xffffff)
    lcd.line(110, 185, 210, 185, color=0xffffff)

def draw_smile_face():
    """绘制专属微笑表情（纯白月牙眼 + 小v嘴）"""
    lcd.fillRect(30, 65, 260, 140, 0x000000) 
    
    # 纯白月牙眼 (利用黑色圆覆盖切割)
    lcd.circle(85, 105, 25, fillcolor=0xffffff, color=0xffffff)
    lcd.circle(85, 118, 25, fillcolor=0x000000, color=0x000000)
    
    lcd.circle(235, 105, 25, fillcolor=0xffffff, color=0xffffff)
    lcd.circle(235, 118, 25, fillcolor=0x000000, color=0x000000)
    
    # 小 v 嘴巴
    lcd.line(150, 175, 160, 190, color=0xffffff)
    lcd.line(160, 190, 170, 175, color=0xffffff)

def draw_yawn_face():
    """绘制打哈欠的表情（纯白眯眯眼 + O型嘴）"""
    lcd.fillRect(30, 65, 260, 140, 0x000000) 
    lcd.line(60, 115, 110, 115, color=0xffffff)
    lcd.line(210, 115, 260, 115, color=0xffffff)
    lcd.circle(160, 185, 15, fillcolor=0x000000, color=0xffffff)

def draw_look_direction(direction):
    """
    绘制左顾右盼的单帧镜像表情
    direction: -1 代表向左看, 1 代表向右看
    """
    lcd.fillRect(30, 65, 260, 140, 0x000000)
    
    # 整体面部偏移量
    offset = direction * 15 
    # 眼球相对于杠的偏移量 (向左看球在左边，向右看球在右边)
    circle_offset = -15 if direction == -1 else 15
    
    # 左眼 (横杠 + 下方小圆球)
    lcd.line(65 + offset, 105, 105 + offset, 105, color=0xffffff)
    # 【已修改】将圆心 Y 坐标从 118 提升至 113，让半径为 8 的圆完美接触 y=105 的横线
    lcd.circle(85 + circle_offset + offset, 113, 8, fillcolor=0xffffff, color=0xffffff)
    
    # 右眼
    lcd.line(215 + offset, 105, 255 + offset, 105, color=0xffffff)
    lcd.circle(235 + circle_offset + offset, 113, 8, fillcolor=0xffffff, color=0xffffff)
    
    # 俏皮的 w 型嘴巴 (由4条线段组成)
    cx = 160 + offset
    cy = 175
    lcd.line(cx - 24, cy - 5, cx - 12, cy + 10, color=0xffffff)
    lcd.line(cx - 12, cy + 10, cx, cy - 2, color=0xffffff)
    lcd.line(cx, cy - 2, cx + 12, cy + 10, color=0xffffff)
    lcd.line(cx + 12, cy + 10, cx + 24, cy - 5, color=0xffffff)

def draw_dizzy_frame(frame):
    """绘制一帧眩晕动画"""
    lcd.fillRect(30, 65, 260, 140, 0x000000) 
    rot_offset = frame * 0.4  
    for cx in [85, 235]: 
        prev_x, prev_y = None, None
        theta = 0.0
        while theta <= 18.8:
            r = 1.6 * theta 
            x = int(cx + r * math.cos(theta + rot_offset))
            y = int(105 + r * math.sin(theta + rot_offset))
            if prev_x is not None:
                lcd.line(prev_x, prev_y, x, y, color=0xffffff)
            prev_x, prev_y = x, y
            theta += 0.6
    wave_offset = int(8 * math.sin(frame * 0.5)) 
    pts = [(110, 185), (135, 175), (160, 195), (185, 175), (210, 185)]
    for i in range(len(pts)-1):
        lcd.line(pts[i][0], pts[i][1] + wave_offset, 
                 pts[i+1][0], pts[i+1][1] + wave_offset, color=0xffffff)

def check_shake():
    """检测摇晃"""
    accel_x, accel_y, accel_z = imu0.acceleration
    if abs(accel_x) > 1.1 or abs(accel_y) > 1.1: return True
    return False

def check_touch():
    """触摸检测"""
    try:
        if touch.status() == 1: return True
    except: pass
    try:
        if btnA.isPressed() or btnB.isPressed() or btnC.isPressed(): return True
    except: pass
    return False

def set_motor(is_on):
    """防错版马达控制"""
    if is_on:
        try: power.setVibration(255)
        except: pass
        try: axp.setLDO3Vol(2800)
        except: pass
        try: axp.setALDO3Vol(3300)
        except: pass
    else:
        try: power.setVibration(0)
        except: pass
        try: axp.setLDO3Vol(0)
        except: pass
        try: axp.setALDO3Vol(0)
        except: pass

# ====================
# 主程序运行逻辑
# ====================

set_motor(False)
draw_idle_face() 
last_action_time = time.ticks_ms()

while True:
    current_time = time.ticks_ms()
    
    # 1. 最高优先级：摇晃 -> 震动眩晕 3 秒
    if check_shake():
        set_motor(True)
        start_time = time.ticks_ms()
        frame_count = 0
        while time.ticks_diff(time.ticks_ms(), start_time) < 3000:
            draw_dizzy_frame(frame_count)
            frame_count += 1
            time.sleep(0.01) 
        set_motor(False)
        draw_idle_face() 
        last_action_time = time.ticks_ms()
        
    # 2. 第二优先级：触碰 -> 纯白微笑 3 秒
    elif check_touch():
        draw_smile_face()
        smile_start_time = time.ticks_ms()
        interrupted = False
        while time.ticks_diff(time.ticks_ms(), smile_start_time) < 3000:
            if check_shake(): # 触摸可被摇晃打断
                interrupted = True
                break
            time.sleep(0.05)
        if not interrupted:
            draw_idle_face()
        last_action_time = time.ticks_ms()
            
    # 3. 第三优先级：放置无操作 20 秒 -> 100% 触发待机动作
    elif time.ticks_diff(current_time, last_action_time) >= 20000:
        # 50% 概率抽签
        if random.randint(1, 2) == 1:
            # --- 动作 A: 打哈欠 (2秒) ---
            draw_yawn_face()
            yawn_start = time.ticks_ms()
            interrupted = False
            while time.ticks_diff(time.ticks_ms(), yawn_start) < 2000:
                if check_shake() or check_touch(): # 可被触摸或摇晃打断
                    interrupted = True
                    break
                time.sleep(0.05)
        else:
            # --- 动作 B: 左顾右盼 (4秒镜像循环) ---
            look_start = time.ticks_ms()
            direction = random.choice([-1, 1]) # 随机决定先看左还是看右
            last_toggle_time = look_start
            draw_look_direction(direction)
            
            interrupted = False
            # 总共持续 4 秒
            while time.ticks_diff(time.ticks_ms(), look_start) < 4000:
                if check_shake() or check_touch(): # 可被触摸或摇晃打断
                    interrupted = True
                    break
                
                # 每 1 秒切换一次镜像方向
                if time.ticks_diff(time.ticks_ms(), last_toggle_time) >= 1000:
                    direction *= -1
                    draw_look_direction(direction)
                    last_toggle_time = time.ticks_ms()
                    
                time.sleep(0.05)
                
        # 动作结束后恢复平淡，并重新开始 20 秒计时
        if not interrupted:
            draw_idle_face()
        last_action_time = time.ticks_ms()
        
    # 4. 最低优先级：待机状态下的随机眨眼 (只在安静时触发)
    elif random.randint(1, 15) == 1:
        lcd.fillRect(60, 80, 50, 50, 0x000000)
        lcd.fillRect(210, 80, 50, 50, 0x000000)
        lcd.line(60, 105, 110, 105, color=0xffffff)
        lcd.line(210, 105, 260, 105, color=0xffffff)
        time.sleep(0.1) 
        draw_idle_face()
        
    time.sleep(0.1)