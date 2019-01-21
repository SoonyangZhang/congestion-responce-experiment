'''
https://blog.csdn.net/qq_29422251/article/details/77713741
'''
def ReadLossInfo(fileName):
    count=0
#cache the last line
    line=""
    for index, line in enumerate(open(fileName,'r')):
        count += 1
    lineArr = line.strip().split()
    return count-1,int(lineArr[2])
instance=9
flows=3;
fileout="loss.txt"    
name="../%s-quic-%s-loss.txt"
fout=open(fileout,'w')
for case in range(instance):
    total_recv=0
    total_loss=0
    average_loss=0.0
    for i in range(flows):
        filename=name%(str(case+1),str(i+1))
        loss,max=ReadLossInfo(filename)
        total_loss+=loss
        total_recv+=max
    average_loss=float(total_loss)/total_recv
    fout.write(str(case+1)+"\t")
    fout.write(str(average_loss)+"\n")    
fout.close()
