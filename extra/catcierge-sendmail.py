from optparse import OptionParser
import argparse
import smtplib

from email.mime.image import MIMEImage
from email.mime.multipart import MIMEMultipart

def send_mail(to_emails, from_email, smtp_server, password, images):
	# Create the container (outer) email message.
	msg = MIMEMultipart()
	msg['Subject'] = 'Catcierge detection'
	msg['From'] = from_email
	msg['To'] = ', '.join(to_emails)
	msg.preamble = 'Catcierge'

	for f in images:
		# Open the files in binary mode.  Let the MIMEImage class automatically
		# guess the specific image type.
		fp = open(f, 'rb')
		img = MIMEImage(fp.read())
		fp.close()
		msg.attach(img)

	# Send the email via our own SMTP server.
	s = smtplib.SMTP(smtp_server)
	s.starttls()
	s.login(from_email, password)
	s.sendmail(from_email, to_emails, msg.as_string())
	s.quit()

def main():
	parser = argparse.ArgumentParser()

	parser.add_argument("--images", metavar = "IMAGES", nargs = "+", 
					help = "The Catcierge match images to send.")

	parser.add_argument("--to", metavar = "TO", nargs = "+",
					help = "List of E-mail addresses to send notification to")

	parser.add_argument("--from", dest = "from_email", metavar = "FROM",
					help = "E-mail to send from")

	parser.add_argument("--smtp", metavar = "SMTP", default = "smtp.gmail.com:587",
					help = "SMTP server to use. Defaults to gmail")

	parser.add_argument("--password", metavar = "PASSW",
					help= "The password to use for SMTP")

	args = parser.parse_args()
	send_mail(args.to, args.from_email, args.smtp, args.password, args.images)

if __name__ == '__main__': main()
