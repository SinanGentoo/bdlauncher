# land.so

/land a 进入起点模式,点地选点  
/land b 进入终点模式，点地选点  
/land exit 退出选点模式  
/land buy 选点之后买地  

/land trust 人名 添加主人  

/land trustgui GUI添加主人  

/land untrust 人名 删除主人  
/land sell 卖地（op可以强行卖）  
/land query 查看当前领地主人  
/land give 人名 转让权限  

<pre>
注意，只有land的第一个owner拥有trust untrust sell perm的权限，trust后的主人只有领地的其他权限。
可以通过land give 转让全部权限给别人
</pre>
/land perm 数字 指定领地具体权限  
<pre>
    PERM_USE=2,  使用方块权限
    PERM_ATK=4,  攻击权限
    PERM_BUILD=8,  建造破坏权限
    PERM_POPITEM=16,  物品展示框权限
    PERM_INTERWITHACTOR=32  盔甲架等交互权限
权限数字为可以使用的权限之和
例如 2=可以睡觉，开箱子
6=可以睡觉，开箱子，攻击生物
</pre>

/reload_land 重载配置  
配置文件 land.json  
制定了领地每一块的价格  
