#ifndef MUTT_DIALOG_H
#define MUTT_DIALOG_H

struct MuttWindow;

struct Dialog
{
  struct MuttWindow *root;
};

void dialog_pop(void);
void dialog_push(struct Dialog *dlg);

#endif /* MUTT_DIALOG_H */
