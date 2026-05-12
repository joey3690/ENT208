Week 3:import M5
import time

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

COLOR_BG = 0x000000
COLOR_FACE = 0xFFFFFF

def face_normal():
    M5.Lcd.clear(COLOR_BG)
    # 微调了Y坐标 (从 CY-15 改为 CY-10)，适应较矮的屏幕
    M5.Lcd.fillCircle(CX - OFFSET, CY - 10, 8, COLOR_FACE)
    M5.Lcd.fillCircle(CX + OFFSET, CY - 10, 8, COLOR_FACE)
    M5.Lcd.drawLine(CX - 15, CY + 20, CX + 15, CY + 20, COLOR_FACE)

def face_happy():
    M5.Lcd.clear(COLOR_BG)
    M5.Lcd.fillCircle(CX - OFFSET, CY - 10, 8, COLOR_FACE)
    M5.Lcd.fillCircle(CX + OFFSET, CY - 10, 8, COLOR_FACE)
    M5.Lcd.drawLine(CX - 15, CY + 15, CX, CY + 25, COLOR_FACE)
    M5.Lcd.drawLine(CX, CY + 25, CX + 15, CY + 15, COLOR_FACE)

def face_sleep():
    M5.Lcd.clear(COLOR_BG)
    M5.Lcd.drawLine(CX - OFFSET - 10, CY - 10, CX - OFFSET + 10, CY - 10, COLOR_FACE)
    M5.Lcd.drawLine(CX + OFFSET - 10, CY - 10, CX + OFFSET + 10, CY - 10, COLOR_FACE)
    M5.Lcd.drawCircle(CX, CY + 20, 4, COLOR_FACE)

def face_dizzy():
    M5.Lcd.clear(COLOR_BG)
    # 画 X 眼睛，调整高度避免出界
    M5.Lcd.drawLine(CX - OFFSET - 10, CY - 20, CX - OFFSET + 10, CY, COLOR_FACE)
    M5.Lcd.drawLine(CX - OFFSET + 10, CY - 20, CX - OFFSET - 10, CY, COLOR_FACE)
    M5.Lcd.drawLine(CX + OFFSET - 10, CY - 20, CX + OFFSET + 10, CY, COLOR_FACE)
    M5.Lcd.drawLine(CX + OFFSET + 10, CY - 20, CX + OFFSET - 10, CY, COLOR_FACE)
    # 画折线嘴巴
    M5.Lcd.drawLine(CX - 15, CY + 25, CX, CY + 15, COLOR_FACE)
    M5.Lcd.drawLine(CX, CY + 15, CX + 15, CY + 25, COLOR_FACE)

# --- 核心逻辑变量 ---
last_change_time = 0      # 记录上一次换表情的时间戳
current_face_index = 0    # 当前轮播到了哪个表情

if __name__ == '__main__':
    # 初始显示正常表情
    face_normal()
    last_change_time = time.ticks_ms() # 记录当前毫秒数
    
    while True:
        # M5.update() 非常重要！它负责刷新硬件状态（按键、传感器等）
        M5.update() 
        
        # 1. 第一优先级：读取加速度计，检查是否摇晃
        acc_x, acc_y, acc_z = M5.Imu.getAccel()
        
        # 正常静止时，重力加速度大概是 1G。如果任何一个方向的加速度绝对值超过 2.0，说明被剧烈摇晃了
        if abs(acc_x) > 2.0 or abs(acc_y) > 2.0 or abs(acc_z) > 2.0:
            face_dizzy()  # 立刻显示眩晕表情
            
            # ---> 新增：StickC Plus 专属蜂鸣器发声 <---
            try:
                # 播放 1000Hz 的声音，持续 200 毫秒 (模拟短促的警报声)
                M5.Speaker.tone(1000, 200) 
            except:
                pass # 如果遇到固件版本差异导致无法发声，安全跳过防止死机
                
            time.sleep(2) # 晕 2 秒
            
            # 晕完之后，重置计时器，准备恢复正常轮播
            last_change_time = time.ticks_ms()
            continue # 跳过下面的代码，直接进入下一次循环
            
        # 2. 第二优先级：“看表”决定是否要换常规表情
        current_time = time.ticks_ms()
        
        # time.ticks_diff 用来计算时间差。如果距离上次换表情过了 2000 毫秒 (2秒)
        if time.ticks_diff(current_time, last_change_time) > 2000:
            current_face_index = (current_face_index + 1) % 3
            
            if current_face_index == 0:
                face_normal()
            elif current_face_index == 1:
                face_happy()
            elif current_face_index == 2:
                face_sleep()
                
            # 换完表情后，把当前时间记入小本本
            last_change_time = current_time
            
        # 每次循环让 CPU 喘口气，50毫秒。这不会造成你感觉到的延迟。
        time.sleep_ms(50)