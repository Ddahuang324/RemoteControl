// LockDialog.cpp: ʵ���ļ�
//
#include "pch.h"
#include "afxdialogex.h"
#include "LockDialog.h"
#include "Resource.h"

// CLockDialog �Ի���

IMPLEMENT_DYNAMIC(CLockDialog, CDialog)

CLockDialog::CLockDialog(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_DIALOG_INFO, pParent)
{

}

CLockDialog::~CLockDialog()
{
}

void CLockDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CLockDialog, CDialog)
END_MESSAGE_MAP()

// CLockDialog ��Ϣ��������
// (�˶Ի�����Ҫ�κ��Զ�����Ϣ����������ֻ��һ��������)