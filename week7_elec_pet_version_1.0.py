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
    """绘制专属微笑表情（白色月牙眼 + 小v嘴 + 红色双杠腮红）"""
    # 局部擦除原有表情和面颊区域
    lcd.fillRect(30, 65, 260, 140, 0x000000) 
    
    # 左眼 (白色月牙)
    # 用纯白色画底圆，再用稍偏下的黑圆切出月牙形状
    lcd.circle(85, 105, 25, fillcolor=0xffffff, color=0xffffff)
    lcd.circle(85, 118, 25, fillcolor=0x000000, color=0x000000)
    
    # 右眼 (白色月牙)
    lcd.circle(235, 105, 25, fillcolor=0xffffff, color=0xffffff)
    lcd.circle(235, 118, 25, fillcolor=0x000000, color=0x000000)
    
    # 左脸颊红色双杠腮红 (采用具有厚度的矩形)
    lcd.fillRect(45, 145, 35, 6, 0xff3333) # 上红杠
    lcd.fillRect(40, 160, 35, 6, 0xff3333) # 下红杠 (略微向外错位)

    # 右脸颊红色双杠腮红
    lcd.fillRect(240, 145, 35, 6, 0xff3333) 
    lcd.fillRect(245, 160, 35, 6, 0xff3333) 
    
    # 开心的小 v 嘴巴
    lcd.line(150, 175, 160, 190, color=0xffffff)
    lcd.line(160, 190, 170, 175, color=0xffffff)

def draw_yawn_face():
    """绘制打哈欠的表情（眯眯眼 + O型嘴）"""
    lcd.fillRect(30, 65, 260, 140, 0x000000) 
    
    lcd.line(60, 115, 110, 115, color=0xffffff)
    lcd.line(210, 115, 260, 115, color=0xffffff)
    lcd.circle(160, 185, 15, fillcolor=0x000000, color=0xffffff)

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
    if abs(accel_x) > 1.1 or abs(accel_y) > 1.1:
        return True
    return False

def check_touch():
    """触摸检测（支持全屏幕与底部虚拟按键）"""
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
        
    # 2. 第二优先级：触碰 -> 专属红晕微笑 3 秒
    elif check_touch():
        draw_smile_face()
        smile_start_time = time.ticks_ms()
        interrupted_by_shake = False
        
        # 持续微笑 3 秒，若期间被摇晃则立刻打断
        while time.ticks_diff(time.ticks_ms(), smile_start_time) < 3000:
            if check_shake():
                interrupted_by_shake = True
                break
            time.sleep(0.05)
            
        if not interrupted_by_shake:
            draw_idle_face()
            
        last_action_time = time.ticks_ms()
            
    # 3. 第三优先级：放置无操作 30 秒 -> 打哈欠 2 秒
    elif time.ticks_diff(current_time, last_action_time) >= 30000:
        draw_yawn_face()
        time.sleep(2)
        draw_idle_face()
        last_action_time = time.ticks_ms()
        
    # 4. 最低优先级：待机状态下的随机眨眼
    elif random.randint(1, 15) == 1:
        lcd.fillRect(60, 80, 50, 50, 0x000000)
        lcd.fillRect(210, 80, 50, 50, 0x000000)
        lcd.line(60, 105, 110, 105, color=0xffffff)
        lcd.line(210, 105, 260, 105, color=0xffffff)
        time.sleep(0.1) 
        draw_idle_face()
        
    time.sleep(0.1)