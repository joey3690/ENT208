from m5stack import *
from m5ui import *
from uiflow import *
import time
import random

# 初始化屏幕
setScreenColor(0x000000)

# 创建两个 UI 图像对象，加载上传的图片
# (x, y, "filename", auto_show)
# 假设 character.png 和 blink.png 已经上传到根目录
img_open = M5Img(0, 0, "/character.png", True)
img_closed = M5Img(0, 0, "/blink.png", True)

# 初始显示睁眼状态
img_closed.hide()
img_open.show()

def draw_character_idle():
    """执行带有随机眨眼逻辑的待机动画"""
    # 随机眨眼逻辑循环
    while True:
        # 等待一个随机时间，在3秒到6秒之间眨一次眼
        wait_time = random.randint(3, 6)
        time.sleep(wait_time)
        
        # 眨眼动作 (闭眼)
        img_open.hide()
        img_closed.show()
        
        # 保持闭眼一段极短的时间 (例如 100 毫秒)
        time.sleep(0.1)
        
        # 恢复睁眼
        img_closed.hide()
        img_open.show()

# 启动待机动画
draw_character_idle()