agetest�� ���ϻ����ԣ����ڻ�ȡ����nandflash ��������������ò�����Ҫ��ʱ��ϳ���һ����Ҫ5Сʱ���ϣ���nandflash�����Ծ�����Ϊ��ȷ�������Բ����鳹�ױ���������Ҫʹ��ǿ�Ʋ����ӿڣ���/sys/kernel/debug/nfc_base/allowEraseBadBlock��Ϊ1����

����˵����
dev:����ָ������ʹ�õ�mtd������Ĭ��ֵΪ4

ʹ�÷�����
1 ��/sys/kernel/debug/nfc_base/allowEraseBadBlock��Ϊ1��
cmd:echo "1" > /sys/kernel/debug/nfc_base/allowEraseBadBlock
2 root����ز���ģ�鲢������������Խ��ͨ��kernel log�����ͨ��erase_time������ĳ��nandflash������;
cmd:insmod nandflash_agetest.ko dev=4
3 ��/sys/kernel/debug/nfc_base/allowEraseBadBlock��Ϊ0��
cmd:echo "0" > /sys/kernel/debug/nfc_base/allowEraseBadBlock

ע�⣺
ÿ����һ�β��ԣ�������һ��bad block��