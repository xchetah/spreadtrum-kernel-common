postest����λ�ò���
��������Ҫ���������Ļ��������Ƿ���ȷ���ò��ԻὫһ��������block��ǳ�bad_block�����Խ�������Ҫ����ǿ�Ʋ����ӿڽ���bad_block�ָ���������/sys/kernel/debug/nfc_base/allowEraseBadBlock��Ϊ1����

����˵����
dev:����ָ������ʹ�õ�mtd������Ĭ��ֵΪ4
bad_pos:����ָ�������ǵ�λ�ã�Ĭ��ֵΪ0
bad_len:����ָ�������ǵĳ��ȣ�Ĭ��ֵΪ2 bytes

ʹ�÷�����
1 ��/sys/kernel/debug/nfc_base/allowEraseBadBlock��Ϊ1��
cmd:echo "1" > /sys/kernel/debug/nfc_base/allowEraseBadBlock
2 root����ز���ģ�鲢������������Խ��ͨ��kernel log���;
cmd:insmod nandflash_postest.ko dev=4 bad_pos=0 bad_len=2
3 ��/sys/kernel/debug/nfc_base/allowEraseBadBlock��Ϊ0
cmd:echo "0" > /sys/kernel/debug/nfc_base/allowEraseBadBlock