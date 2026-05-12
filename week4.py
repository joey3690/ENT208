Week4:import M5
import time
import math  # 新增：引入数学库用来计算波浪和螺旋

M5.begin()

# --- M5StickC Plus 专属设置 ---
# 1表示顺时针旋转90度，变成横屏 (大按键A在右侧)
M5.Lcd.setRotation(1) 

# 自动获取当前横屏的宽高 (此时 SW=240, SH=135)
SW = M5.Lcd.width()
SH = M5.Lcd.height()
CX = SW // 2
CY = SH // 2
OFFSET = SW // 4

# --- 颜色设置 ---
COLOR_BG = 0x000000      # 黑色背景
COLOR_EYE = 0x0000FF     # 纯蓝色 (眼睛)
COLOR_MOUTH = 0xFF0000   # 纯红色 (嘴巴)

def face_normal():
    M5.Lcd.clear(COLOR_BG)
    M5.Lcd.fillCircle(CX - OFFSET, CY - 10, 8, COLOR_EYE)
    M5.Lcd.fillCircle(CX + OFFSET, CY - 10, 8, COLOR_EYE)
    M5.Lcd.drawLine(CX - 15, CY + 20, CX + 15, CY + 20, COLOR_MOUTH)

def face_happy():
    M5.Lcd.clear(COLOR_BG)
    M5.Lcd.fillCircle(CX - OFFSET, CY - 10, 8, COLOR_EYE)
    M5.Lcd.fillCircle(CX + OFFSET, CY - 10, 8, COLOR_EYE)
    M5.Lcd.drawLine(CX - 15, CY + 15, CX, CY + 25, COLOR_MOUTH)
    M5.Lcd.drawLine(CX, CY + 25, CX + 15, CY + 15, COLOR_MOUTH)

def face_sleep():
    M5.Lcd.clear(COLOR_BG)
    M5.Lcd.drawLine(CX - OFFSET - 10, CY - 10, CX - OFFSET + 10, CY - 10, COLOR_EYE)
    M5.Lcd.drawLine(CX + OFFSET - 10, CY - 10, CX + OFFSET + 10, CY - 10, COLOR_EYE)
    M5.Lcd.drawCircle(CX, CY + 20, 4, COLOR_MOUTH)


# --------- 高级动画绘制函数 (替换原来的静态 dizzy) ---------
def draw_swirl_eye(xc, yc, phase):
    """绘制单只旋转的蚊香眼"""
    prev_x, prev_y = xc, yc
    for i in range(1, 35):
        angle = i * 0.5 + phase 
        radius = i * 0.4     
        
        x = int(xc + math.cos(angle) * radius)
        y = int(yc + math.sin(angle) * radius)
        M5.Lcd.drawLine(prev_x, prev_y, x, y, COLOR_EYE) # 使用蓝眼睛颜色
        prev_x, prev_y = x, y

def draw_wave_mouth(xc, yc, phase):
    """绘制波纹浮动的嘴巴"""
    width = 40  
    start_x = xc - width // 2
    
    freq = 1.2  # 【波浪密集度】：修改为 1.2，波纹变得更多更密
    amp = 6     
    
    prev_x = start_x
    prev_y = int(yc + math.sin((start_x * freq) + phase) * amp)

    for x in range(start_x + 2, start_x + width + 1, 2):
        y = int(yc + math.sin((x * freq) + phase) * amp)
        M5.Lcd.drawLine(prev_x, prev_y, x, y, COLOR_MOUTH) # 使用红嘴巴颜色
        prev_x, prev_y = x, y


# --- 核心逻辑变量 ---
last_change_time = 0      # 记录上一次换表情的时间戳
current_face_index = 0    # 当前轮播到了哪个表情

if __name__ == '__main__':
    # 初始显示正常表情
    face_normal()
    last_change_time = time.ticks_ms() 
    
    while True:
        M5.update() 
        
        # 1. 读取加速度计，检查是否摇晃
        acc_x, acc_y, acc_z = M5.Imu.getAccel()
        
        if abs(acc_x) > 2.0 or abs(acc_y) > 2.0 or abs(acc_z) > 2.0:
            # 蜂鸣器警报音
            try:
                M5.Speaker.tone(1000, 200) 
            except:
                pass 
                
            # --- 眩晕动画引擎启动 ---
            dizzy_start_time = time.ticks_ms()
            t = 0.0 # 动画相位
            
            # 持续 3000 毫秒的动态刷新循环
            while time.ticks_diff(time.ticks_ms(), dizzy_start_time) < 3000:
                M5.Lcd.clear(COLOR_BG)
                
                draw_swirl_eye(CX - OFFSET, CY - 10, t)
                draw_swirl_eye(CX + OFFSET, CY - 10, t)
                draw_wave_mouth(CX, CY + 25, t * 2)
                
                t += 0.3 
                time.sleep_ms(30) # 控制在约 30fps 刷新
            # --- 眩晕动画引擎结束 ---
            
            last_change_time = time.ticks_ms()
            continue 
            
        # 2. 自动轮播常规表情
        current_time = time.ticks_ms()
        if time.ticks_diff(current_time, last_change_time) > 2000:
            current_face_index = (current_face_index + 1) % 3
            
            if current_face_index == 0:
                face_normal()
            elif current_face_index == 1:
                face_happy()
            elif current_face_index == 2:
                face_sleep()
                
            last_change_time = current_time
            
        time.sleep_ms(50)
