from m5stack import *
from m5ui import *
from uiflow import *

# 初始化屏幕
setScreenColor(0x000000) # 背景设为黑色

def draw_neutral_face():
    # 清除屏幕（可选，如果需要刷新）
    # lcd.clear()
    
    # 绘制左眼 (x, y, r, color)
    lcd.circle(80, 100, 20, fillcolor=0xffffff, color=0xffffff)
    
    # 绘制右眼 (x, y, r, color)
    lcd.circle(240, 100, 20, fillcolor=0xffffff, color=0xffffff)
    
    # 绘制嘴巴 (x1, y1, x2, y2, color)
    # 用一条简单的直线表示平淡
    lcd.line(110, 180, 210, 180, color=0xffffff)

# 执行绘制
draw_neutral_face()